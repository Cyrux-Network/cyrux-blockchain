//! The Phat Contract registry

pub use self::pallet::*;

#[frame_support::pallet]
pub mod pallet {
	#![allow(clippy::too_many_arguments)]

	use codec::Encode;
	use frame_support::{
		dispatch::DispatchResult,
		pallet_prelude::*,
		traits::{Currency, ExistenceRequirement, StorageVersion},
	};
	use frame_system::pallet_prelude::*;
	use sp_core::crypto::UncheckedFrom;
	use sp_core::H256;
	use sp_runtime::{
		traits::{UniqueSaturatedInto, Zero},
		AccountId32,
	};
	use sp_std::prelude::*;

	use crate::{
		mq::{IntoH256, MessageOriginInfo, Pallet as PalletMq},
		registry,
	};
	use cyrux_types::{
		contract::{
			command_topic,
			messaging::{
				ClusterEvent, ClusterOperation, ContractOperation, ResourceType,
				WorkerClusterReport,
			},
			ClusterInfo, ClusterPermission, CodeIndex, ContractClusterId, ContractId, ContractInfo,
		},
		messaging::{bind_topic, DecodedMessage, MessageOrigin},
		ClusterPublicKey, ContractPublicKey, WorkerIdentity, WorkerPublicKey,
	};

	type BalanceOf<T> =
		<<T as Config>::Currency as Currency<<T as frame_system::Config>::AccountId>>::Balance;
	#[derive(Encode, Decode, Clone, Debug, TypeInfo)]
	pub struct BasicContractInfo {
		pub deployer: AccountId32,
		pub cluster: ContractClusterId,
	}

	bind_topic!(ClusterRegistryEvent, b"^cyrux/registry/cluster");
	#[derive(Encode, Decode, Clone, Debug)]
	pub enum ClusterRegistryEvent {
		PubkeyAvailable {
			cluster: ContractClusterId,
			pubkey: ClusterPublicKey,
		},
	}

	bind_topic!(ContractRegistryEvent, b"^cyrux/registry/contract");
	#[derive(Encode, Decode, Clone, Debug)]
	pub enum ContractRegistryEvent {
		PubkeyAvailable {
			contract: ContractId,
			pubkey: ContractPublicKey,
			deployer: ContractId,
		},
	}

	#[pallet::config]
	pub trait Config: frame_system::Config {
		type RuntimeEvent: From<Event<Self>> + IsType<<Self as frame_system::Config>::RuntimeEvent>;
		type InkCodeSizeLimit: Get<u32>;
		type SidevmCodeSizeLimit: Get<u32>;
		type Currency: Currency<Self::AccountId>;
	}

	const STORAGE_VERSION: StorageVersion = StorageVersion::new(8);

	#[pallet::pallet]
	#[pallet::storage_version(STORAGE_VERSION)]
	#[pallet::without_storage_info]
	pub struct Pallet<T>(_);

	#[pallet::storage]
	pub type Contracts<T: Config> = StorageMap<_, Twox64Concat, ContractId, BasicContractInfo>;

	/// The contract cluster counter, it always equals to the latest cluster id.
	#[pallet::storage]
	pub type ClusterCounter<T> = StorageValue<_, u64, ValueQuery>;

	#[pallet::storage]
	pub type Clusters<T: Config> =
		StorageMap<_, Twox64Concat, ContractClusterId, ClusterInfo<T::AccountId>>;

	#[pallet::storage]
	pub type ClusterContracts<T: Config> =
		StorageMap<_, Twox64Concat, ContractClusterId, Vec<ContractId>, ValueQuery>;

	#[pallet::storage]
	pub type ClusterWorkers<T> =
		StorageMap<_, Twox64Concat, ContractClusterId, Vec<WorkerPublicKey>, ValueQuery>;

	#[pallet::storage]
	pub type ClusterByWorkers<T> = StorageMap<_, Twox64Concat, WorkerPublicKey, ContractClusterId>;

	/// The pink-system contract code used to deploy new clusters
	#[pallet::storage]
	pub type PinkSystemCode<T> = StorageValue<_, (u16, Vec<u8>), ValueQuery>;
	/// The blake2_256 hash of the pink-system contract code.
	#[pallet::storage]
	pub type PinkSystemCodeHash<T> = StorageValue<_, H256, OptionQuery>;
	/// The pink-runtime version used to deploy new clusters.
	/// See also: `phactory::storage::pink_runtime_version`.
	#[pallet::storage]
	pub type PinkRuntimeVersion<T> = StorageValue<_, (u32, u32)>;

	/// The next pink-system contract code to be applied from the next block
	#[pallet::storage]
	pub type NextPinkSystemCode<T> = StorageValue<_, Vec<u8>, OptionQuery>;

	#[pallet::event]
	#[pallet::generate_deposit(pub(super) fn deposit_event)]
	pub enum Event<T: Config> {
		ClusterCreated {
			cluster: ContractClusterId,
			system_contract: ContractId,
		},
		ClusterPubkeyAvailable {
			cluster: ContractClusterId,
			pubkey: ClusterPublicKey,
		},
		ClusterDeployed {
			cluster: ContractClusterId,
			pubkey: ClusterPublicKey,
			worker: WorkerPublicKey,
		},
		ClusterDeploymentFailed {
			cluster: ContractClusterId,
			worker: WorkerPublicKey,
		},
		Instantiating {
			contract: ContractId,
			cluster: ContractClusterId,
			deployer: T::AccountId,
		},
		ContractPubkeyAvailable {
			contract: ContractId,
			cluster: ContractClusterId,
			pubkey: ContractPublicKey,
		},
		Instantiated {
			contract: ContractId,
			cluster: ContractClusterId,
			deployer: H256,
		},
		ClusterDestroyed {
			cluster: ContractClusterId,
		},
		Transfered {
			cluster: ContractClusterId,
			account: H256,
			amount: BalanceOf<T>,
		},
		WorkerAddedToCluster {
			worker: WorkerPublicKey,
			cluster: ContractClusterId,
		},
		WorkerRemovedFromCluster {
			worker: WorkerPublicKey,
			cluster: ContractClusterId,
		},
	}

	#[pallet::error]
	pub enum Error<T> {
		CodeNotFound,
		ClusterNotFound,
		ClusterNotDeployed,
		ClusterPermissionDenied,
		DuplicatedContract,
		DuplicatedDeployment,
		NoWorkerSpecified,
		InvalidSender,
		WorkerNotFound,
		PayloadTooLarge,
		NoPinkSystemCode,
		ContractNotFound,
		WorkerIsBusy,
	}

	type CodeHash<T> = <T as frame_system::Config>::Hash;

	fn check_cluster_permission<T: Config>(
		deployer: &T::AccountId,
		cluster: &ClusterInfo<T::AccountId>,
	) -> bool {
		match &cluster.permission {
			ClusterPermission::Public => true,
			ClusterPermission::OnlyOwner(owner) => deployer == owner,
		}
	}

	#[pallet::call]
	impl<T: Config> Pallet<T>
	where
		T: crate::mq::Config + crate::registry::Config,
		T: frame_system::Config<AccountId = AccountId32>,
	{
		/// Create a new cluster
		///
		/// # Arguments
		/// - `owner` - The owner of the cluster.
		/// - `permission` - Who can deploy contracts in the cluster.
		/// - `deploy_workers` - Workers included in the cluster.
		/// - `deposit` - Transfer amount of tokens from the owner on chain to the owner in cluster.
		/// - `gas_price` - Gas price for contract transactions.
		/// - `deposit_per_item` - Price for contract storage per item.
		/// - `deposit_per_byte` - Price for contract storage per byte.
		/// - `treasury_account` - The treasury account used to collect the gas and storage fee.
		#[pallet::call_index(0)]
		#[pallet::weight({0})]
		pub fn add_cluster(
			origin: OriginFor<T>,
			owner: T::AccountId,
			permission: ClusterPermission<T::AccountId>,
			deploy_workers: Vec<WorkerPublicKey>,
			deposit: BalanceOf<T>,
			gas_price: BalanceOf<T>,
			deposit_per_item: BalanceOf<T>,
			deposit_per_byte: BalanceOf<T>,
			treasury_account: AccountId32,
		) -> DispatchResult {
			T::GovernanceOrigin::ensure_origin(origin)?;

			ensure!(!deploy_workers.is_empty(), Error::<T>::NoWorkerSpecified);
			let workers = deploy_workers
				.iter()
				.map(|worker| {
					let worker_info =
						registry::Workers::<T>::get(worker).ok_or(Error::<T>::WorkerNotFound)?;
					Ok(WorkerIdentity {
						pubkey: worker_info.pubkey,
						ecdh_pubkey: worker_info.ecdh_pubkey,
					})
				})
				.collect::<Result<Vec<WorkerIdentity>, Error<T>>>()?;

			let cluster_id = ClusterCounter::<T>::mutate(|counter| {
				// 0 is reserved for removed workers
				*counter += 1;
				*counter
			});
			let cluster = ContractClusterId::from_low_u64_be(cluster_id);

			for worker in &deploy_workers {
				ensure!(
					ClusterByWorkers::<T>::get(worker).is_none(),
					Error::<T>::WorkerIsBusy
				);
				ClusterByWorkers::<T>::insert(worker, cluster);
			}
			ClusterWorkers::<T>::insert(cluster, deploy_workers);

			let system_code_hash =
				PinkSystemCodeHash::<T>::get().ok_or(Error::<T>::NoPinkSystemCode)?;
			let selector = vec![0xed, 0x4b, 0x9d, 0x1b]; // The default() constructor
			let system_contract_info = ContractInfo {
				deployer: owner.clone(),
				code_index: CodeIndex::WasmCode(system_code_hash),
				salt: Default::default(),
				cluster_id: cluster,
				instantiate_data: selector,
			};

			let system_contract = system_contract_info.contract_id(crate::hashing::blake2_256);

			let cluster_info = ClusterInfo {
				owner: owner.clone(),
				permission,
				system_contract,
				gas_price: gas_price.unique_saturated_into(),
				deposit_per_item: deposit_per_item.unique_saturated_into(),
				deposit_per_byte: deposit_per_byte.unique_saturated_into(),
			};

			Clusters::<T>::insert(cluster, cluster_info);
			Self::deposit_event(Event::ClusterCreated {
				cluster,
				system_contract,
			});
			<T as Config>::Currency::transfer(
				&owner,
				&cluster_account(&cluster),
				deposit,
				ExistenceRequirement::KeepAlive,
			)?;
			Self::push_message(ClusterEvent::DeployCluster {
				owner,
				cluster,
				workers,
				deposit: deposit.unique_saturated_into(),
				gas_price: gas_price.unique_saturated_into(),
				deposit_per_item: deposit_per_item.unique_saturated_into(),
				deposit_per_byte: deposit_per_byte.unique_saturated_into(),
				treasury_account,
			});
			Ok(())
		}

		#[pallet::call_index(1)]
		#[pallet::weight({0})]
		pub fn cluster_upload_resource(
			origin: OriginFor<T>,
			cluster_id: ContractClusterId,
			resource_type: ResourceType,
			resource_data: Vec<u8>,
		) -> DispatchResult {
			let origin: T::AccountId = ensure_signed(origin)?;
			let cluster_info = Clusters::<T>::get(cluster_id).ok_or(Error::<T>::ClusterNotFound)?;
			ensure!(
				check_cluster_permission::<T>(&origin, &cluster_info),
				Error::<T>::ClusterPermissionDenied
			);

			let size_limit = match resource_type {
				ResourceType::InkCode => T::InkCodeSizeLimit::get(),
				ResourceType::SidevmCode => T::SidevmCodeSizeLimit::get(),
				ResourceType::IndeterministicInkCode => T::InkCodeSizeLimit::get(),
			} as usize;
			ensure!(
				resource_data.len() <= size_limit,
				Error::<T>::PayloadTooLarge
			);

			Self::push_message(ClusterOperation::UploadResource {
				origin,
				cluster_id,
				resource_type,
				resource_data,
			});
			Ok(())
		}

		/// Transfers some native token to an account in a Phat Contract cluster.
		///
		/// The token will be deducted from the sender's account, and transfer to the specified
		/// account in the cluster, for token usage (gas, storage deposit, etc) inside the cluster.
		/// Please note that currently it's only supported to transfer token from the blockchain
		/// into the cluster, not the reverse.
		///
		/// # Arguments
		///
		/// * `amount` - The amount of the native token to transfer.
		/// * `cluster_id` - The cluster id to transfer into.
		/// * `dest_account` - The account in the cluster to receive the token.
		#[pallet::call_index(2)]
		#[pallet::weight({0})]
		pub fn transfer_to_cluster(
			origin: OriginFor<T>,
			amount: BalanceOf<T>,
			cluster_id: ContractClusterId,
			dest_account: AccountId32,
		) -> DispatchResult {
			let user = ensure_signed(origin)?;
			<T as Config>::Currency::transfer(
				&user,
				&cluster_account(&cluster_id),
				amount,
				ExistenceRequirement::KeepAlive,
			)?;
			Self::push_message(ClusterOperation::Deposit {
				cluster_id,
				account: dest_account.clone().into_h256(),
				amount: amount.unique_saturated_into(),
			});
			Self::deposit_event(Event::Transfered {
				cluster: cluster_id,
				account: dest_account.into_h256(),
				amount,
			});
			Ok(())
		}

		// Push message to contract with some deposit into the cluster to pay the gas fee
		#[pallet::call_index(3)]
		#[pallet::weight(Weight::from_parts(10_000u64, 0))]
		pub fn push_contract_message(
			origin: OriginFor<T>,
			contract_id: ContractId,
			payload: Vec<u8>,
			deposit: BalanceOf<T>,
		) -> DispatchResult {
			let user = ensure_signed(origin.clone())?;
			if !deposit.is_zero() {
				let contract_info =
					Contracts::<T>::get(contract_id).ok_or(Error::<T>::ContractNotFound)?;
				Self::transfer_to_cluster(origin.clone(), deposit, contract_info.cluster, user)?;
			}
			PalletMq::<T>::push_message(origin, command_topic(contract_id), payload)
		}

		#[pallet::call_index(4)]
		#[pallet::weight({0})]
		pub fn instantiate_contract(
			origin: OriginFor<T>,
			code_index: CodeIndex<CodeHash<T>>,
			data: Vec<u8>,
			salt: Vec<u8>,
			cluster_id: ContractClusterId,
			transfer: BalanceOf<T>,
			gas_limit: u64,
			storage_deposit_limit: Option<BalanceOf<T>>,
			deposit: BalanceOf<T>,
		) -> DispatchResult {
			let deployer = ensure_signed(origin.clone())?;
			let cluster_info = Clusters::<T>::get(cluster_id).ok_or(Error::<T>::ClusterNotFound)?;
			ensure!(
				check_cluster_permission::<T>(&deployer, &cluster_info),
				Error::<T>::ClusterPermissionDenied
			);

			if !deposit.is_zero() {
				Self::transfer_to_cluster(origin.clone(), deposit, cluster_id, deployer.clone())?;
			}

			let contract_info = ContractInfo {
				deployer,
				code_index,
				salt,
				cluster_id,
				instantiate_data: data,
			};
			let contract_id = contract_info.contract_id(crate::hashing::blake2_256);
			ensure!(
				!Contracts::<T>::contains_key(contract_id),
				Error::<T>::DuplicatedContract
			);
			Contracts::<T>::insert(
				contract_id,
				BasicContractInfo {
					deployer: contract_info.deployer.clone(),
					cluster: contract_info.cluster_id,
				},
			);

			Self::push_message(ContractOperation::instantiate_code(
				contract_info.clone(),
				transfer.unique_saturated_into(),
				gas_limit,
				storage_deposit_limit.map(UniqueSaturatedInto::unique_saturated_into),
			));
			Self::deposit_event(Event::Instantiating {
				contract: contract_id,
				cluster: contract_info.cluster_id,
				deployer: contract_info.deployer,
			});

			Ok(())
		}

		#[pallet::call_index(5)]
		#[pallet::weight({0})]
		pub fn cluster_destroy(origin: OriginFor<T>, cluster: ContractClusterId) -> DispatchResult {
			ensure_root(origin)?;

			Clusters::<T>::take(cluster).ok_or(Error::<T>::ClusterNotFound)?;
			Self::push_message(ClusterOperation::<T::AccountId>::DestroyCluster(cluster));
			Self::deposit_event(Event::ClusterDestroyed { cluster });
			Ok(())
		}

		#[pallet::call_index(6)]
		#[pallet::weight({0})]
		pub fn set_pink_system_code(
			origin: OriginFor<T>,
			code: BoundedVec<u8, T::InkCodeSizeLimit>,
		) -> DispatchResult {
			T::GovernanceOrigin::ensure_origin(origin)?;
			NextPinkSystemCode::<T>::put(code);
			Ok(())
		}

		#[pallet::call_index(7)]
		#[pallet::weight({0})]
		pub fn set_pink_runtime_version(
			origin: OriginFor<T>,
			version: (u32, u32),
		) -> DispatchResult {
			T::GovernanceOrigin::ensure_origin(origin)?;
			PinkRuntimeVersion::<T>::put(version);
			Ok(())
		}

		/// Add a new worker to a cluster
		#[pallet::call_index(8)]
		#[pallet::weight({0})]
		pub fn add_worker_to_cluster(
			origin: OriginFor<T>,
			worker_pubkey: WorkerPublicKey,
			cluster_id: ContractClusterId,
		) -> DispatchResult {
			let origin = ensure_signed(origin)?;
			let cluster_info = Clusters::<T>::get(cluster_id).ok_or(Error::<T>::ClusterNotFound)?;
			ensure!(
				cluster_info.owner == origin,
				Error::<T>::ClusterPermissionDenied
			);
			ensure!(
				registry::Workers::<T>::get(worker_pubkey).is_some(),
				Error::<T>::WorkerNotFound
			);
			ensure!(
				ClusterByWorkers::<T>::get(worker_pubkey).is_none(),
				Error::<T>::WorkerIsBusy
			);
			// TODO: Do we need to check whether the worker agree to join the cluster?
			ClusterByWorkers::<T>::insert(worker_pubkey, cluster_id);
			ClusterWorkers::<T>::append(cluster_id, worker_pubkey);
			Clusters::<T>::insert(cluster_id, cluster_info);
			Self::deposit_event(Event::WorkerAddedToCluster {
				worker: worker_pubkey,
				cluster: cluster_id,
			});
			Ok(())
		}

		/// Remove a new worker from a cluster
		#[pallet::call_index(9)]
		#[pallet::weight({0})]
		pub fn remove_worker_from_cluster(
			origin: OriginFor<T>,
			worker_pubkey: WorkerPublicKey,
			cluster_id: ContractClusterId,
		) -> DispatchResult {
			let origin = ensure_signed(origin)?;
			let cluster_info = Clusters::<T>::get(cluster_id).ok_or(Error::<T>::ClusterNotFound)?;
			ensure!(
				cluster_info.owner == origin,
				Error::<T>::ClusterPermissionDenied
			);
			ensure!(
				ClusterByWorkers::<T>::get(worker_pubkey) == Some(cluster_id),
				Error::<T>::WorkerNotFound
			);
			// Put the worker to cluster 0 to avoid it to be added to some cluster again.
			ClusterByWorkers::<T>::insert(worker_pubkey, ContractClusterId::from_low_u64_be(0));
			ClusterWorkers::<T>::mutate(cluster_id, |workers| {
				workers.retain(|key| key != &worker_pubkey)
			});
			Self::deposit_event(Event::WorkerRemovedFromCluster {
				worker: worker_pubkey,
				cluster: cluster_id,
			});
			Self::push_message(ClusterOperation::<T::AccountId>::RemoveWorker {
				cluster_id,
				worker: worker_pubkey,
			});
			Ok(())
		}

		/// Cleanup the removed workers in ClusterWorkers which is mis-added back by the migration
		#[pallet::call_index(10)]
		#[pallet::weight({0})]
		pub fn cleanup_removed_workers(
			origin: OriginFor<T>,
			cluster_id: ContractClusterId,
		) -> DispatchResult {
			ensure_root(origin)?;
			ClusterWorkers::<T>::mutate(cluster_id, |workers| {
				workers.retain(|key| {
					ClusterByWorkers::<T>::get(key) != Some(ContractClusterId::from_low_u64_be(0))
				})
			});
			Ok(())
		}
	}

	impl<T: Config> Pallet<T>
	where
		T: crate::mq::Config + crate::registry::Config,
	{
		pub fn on_cluster_message_received(
			message: DecodedMessage<ClusterRegistryEvent>,
		) -> DispatchResult {
			ensure!(
				message.sender == MessageOrigin::Gatekeeper,
				Error::<T>::InvalidSender
			);
			match message.payload {
				ClusterRegistryEvent::PubkeyAvailable { cluster, pubkey } => {
					// The cluster key can be over-written with the latest value by Gatekeeper
					registry::ClusterKeys::<T>::insert(cluster, pubkey);
					Self::deposit_event(Event::ClusterPubkeyAvailable { cluster, pubkey });
				}
			}
			Ok(())
		}

		pub fn on_contract_message_received(
			message: DecodedMessage<ContractRegistryEvent>,
		) -> DispatchResult {
			let cluster = match message.sender {
				MessageOrigin::Cluster(cluster) => cluster,
				_ => return Err(Error::<T>::InvalidSender.into()),
			};
			match message.payload {
				ContractRegistryEvent::PubkeyAvailable {
					contract,
					pubkey,
					deployer,
				} => {
					registry::ContractKeys::<T>::insert(contract, pubkey);
					Self::deposit_event(Event::ContractPubkeyAvailable {
						contract,
						cluster,
						pubkey,
					});
					ClusterContracts::<T>::append(cluster, contract);
					Contracts::<T>::mutate(contract, |info| {
						// If the info is Some, it was instantiated by user.
						if info.is_none() {
							*info = Some(BasicContractInfo {
								deployer: AccountId32::from(deployer.0),
								cluster,
							});
						}
					});
					Self::deposit_event(Event::Instantiated {
						contract,
						cluster,
						deployer,
					});
				}
			}
			Ok(())
		}

		pub fn on_worker_cluster_message_received(
			message: DecodedMessage<WorkerClusterReport>,
		) -> DispatchResult {
			let worker_pubkey = match message.sender {
				MessageOrigin::Worker(worker_pubkey) => worker_pubkey,
				_ => return Err(Error::<T>::InvalidSender.into()),
			};
			match message.payload {
				WorkerClusterReport::ClusterDeployed { id, pubkey } => {
					// TODO.shelven: scalability concern for large number of workers
					Self::deposit_event(Event::ClusterDeployed {
						cluster: id,
						pubkey,
						worker: worker_pubkey,
					});
				}
				WorkerClusterReport::ClusterDeploymentFailed { id } => {
					Self::deposit_event(Event::ClusterDeploymentFailed {
						cluster: id,
						worker: worker_pubkey,
					});
				}
			}
			Ok(())
		}

		pub fn get_system_contract(contract: &ContractId) -> Option<ContractId> {
			let contract_info = Contracts::<T>::get(contract)?;
			let cluster_info = Clusters::<T>::get(contract_info.cluster)?;
			Some(cluster_info.system_contract)
		}

		pub fn get_contract_info(contract: &ContractId) -> Option<BasicContractInfo> {
			Contracts::<T>::get(contract)
		}
	}

	#[pallet::hooks]
	impl<T: Config> Hooks<BlockNumberFor<T>> for Pallet<T> {
		fn on_initialize(_now: BlockNumberFor<T>) -> Weight {
			let Some(next_code) = NextPinkSystemCode::<T>::take() else {
				return Weight::zero();
			};
			let hash: H256 = crate::hashing::blake2_256(&next_code).into();
			PinkSystemCodeHash::<T>::put(hash);
			PinkSystemCode::<T>::mutate(|(ver, code)| {
				*ver += 1;
				*code = next_code;
			});
			Weight::zero()
		}
		fn on_runtime_upgrade() -> Weight {
			use frame_support::traits::OnRuntimeUpgrade;
			migration::Migration::<T>::on_runtime_upgrade()
		}
	}

	impl<T: Config + crate::mq::Config> MessageOriginInfo for Pallet<T> {
		type Config = T;
	}

	pub fn cluster_account(cluster_id: &ContractClusterId) -> AccountId32 {
		let mut buf = b"cluster:".to_vec();
		buf.extend(cluster_id.as_ref());
		AccountId32::unchecked_from(crate::hashing::blake2_256(&buf).into())
	}

	mod migration {
		use super::*;
		use frame_support::traits::OnRuntimeUpgrade;
		use sp_std::marker::PhantomData;

		pub struct Migration<T>(PhantomData<T>);

		impl<T: Config + frame_system::Config> OnRuntimeUpgrade for Migration<T> {
			fn on_runtime_upgrade() -> Weight {
				migrate_v7_to_v8::<T>()
			}
		}

		fn migrate_v7_to_v8<T: Config + frame_system::Config>() -> Weight {
			let onchain_version = Pallet::<T>::on_chain_storage_version();
			let mut nreads = 0;

			if onchain_version < 8 {
				#[derive(Encode, Decode, Clone, PartialEq, Eq, Debug, TypeInfo)]
				pub struct ClusterInfoV7<AccountId> {
					pub owner: AccountId,
					pub permission: ClusterPermission<AccountId>,
					pub workers: Vec<WorkerPublicKey>,
					pub system_contract: ContractId,
					pub gas_price: u128,
					pub deposit_per_item: u128,
					pub deposit_per_byte: u128,
				}

				Clusters::<T>::translate(|id, old: ClusterInfoV7<T::AccountId>| {
					nreads += 2;
					let mut workers = ClusterWorkers::<T>::get(id);
					for worker in old.workers {
						if !workers.contains(&worker)
							&& ClusterByWorkers::<T>::get(worker)
								!= Some(ContractClusterId::from_low_u64_be(0))
						{
							workers.push(worker);
						}
					}
					ClusterWorkers::<T>::insert(id, workers);
					Some(ClusterInfo {
						owner: old.owner,
						permission: old.permission,
						system_contract: old.system_contract,
						gas_price: old.gas_price,
						deposit_per_item: old.deposit_per_item,
						deposit_per_byte: old.deposit_per_byte,
					})
				});

				StorageVersion::new(8).put::<Pallet<T>>();
			}
			// Roughly estimate the weight of this migration
			T::DbWeight::get().reads_writes(nreads, nreads)
		}
	}
}
