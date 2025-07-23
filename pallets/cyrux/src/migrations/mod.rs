#[allow(unused_imports)]
use frame_support::{
	traits::{
		tokens::fungibles::{Inspect, Mutate},
		Currency,
		ExistenceRequirement::{AllowDeath, KeepAlive},
		Get, LockIdentifier, LockableCurrency, StorageVersion,
	},
	weights::Weight,
	BoundedVec, Twox64Concat,
};
#[allow(unused_imports)]
use log;

use crate::compute::{base_pool, computation, stake_pool_v2, vault, wrapped_balances};
use crate::mq;
use crate::phat;
use crate::registry;

/// Alias for the runtime that implements all cyrux Pallets
pub trait cyruxPallets:
	phat::Config
	+ frame_system::Config
	+ computation::Config
	+ mq::Config
	+ registry::Config
	+ stake_pool_v2::Config
	+ base_pool::Config
	+ vault::Config
	+ crate::cyruxConfig
{
}
impl<T> cyruxPallets for T where
	T: phat::Config
		+ frame_system::Config
		+ computation::Config
		+ mq::Config
		+ registry::Config
		+ stake_pool_v2::Config
		+ base_pool::Config
		+ vault::Config
		+ wrapped_balances::Config
		+ crate::cyruxConfig
{
}

type Versions = (
	StorageVersion,
	StorageVersion,
	StorageVersion,
	StorageVersion,
	StorageVersion,
);

#[allow(dead_code)]
fn get_versions<T: cyruxPallets>() -> Versions {
	(
		StorageVersion::get::<phat::Pallet<T>>(),
		StorageVersion::get::<computation::Pallet<T>>(),
		StorageVersion::get::<mq::Pallet<T>>(),
		StorageVersion::get::<registry::Pallet<T>>(),
		StorageVersion::get::<stake_pool_v2::Pallet<T>>(),
	)
}

#[allow(dead_code)]
fn unified_versions(version: u16) -> Versions {
	(
		StorageVersion::new(version),
		StorageVersion::new(version),
		StorageVersion::new(version),
		StorageVersion::new(version),
		StorageVersion::new(version),
	)
}

#[allow(dead_code)]
fn set_unified_version<T: cyruxPallets>(version: u16) {
	StorageVersion::new(version).put::<phat::Pallet<T>>();
	StorageVersion::new(version).put::<computation::Pallet<T>>();
	StorageVersion::new(version).put::<mq::Pallet<T>>();
	StorageVersion::new(version).put::<registry::Pallet<T>>();
	StorageVersion::new(version).put::<stake_pool_v2::Pallet<T>>();
}
