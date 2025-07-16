const { ApiPromise, WsProvider, Keyring } = require('@polkadot/api');
// const {
//     createKeyMulti,
//     encodeAddress,
//     // sortAddresses
// } = require('@polkadot/util-crypto');
const BN = require('bn.js');

const { program } = require('commander');

const bn1e12 = new BN(10).pow(new BN(12));
const bn0 = new BN(0);

const Logger = {
    info(...args) {
        const t = new Date().toLocaleString();
        process.stdout.write(`[${t}] `);
        console.info(...args);
    },
    warn(...args) {
        const t = new Date().toLocaleString();
        process.stderr.write(`[${t}] `);
        console.warn(...args);
    }
};

program
    .option('--floor <amount>', 'floor price.', '1000')
    .option('--ceil <amount>', 'ceil price', '8000')
    .option('--margin <amount>', 'margin to the other bidder', '10')
    .option('--interval <ms>', 'bidding interval in ms', '6000')
    .option('--funding <account>', 'funding account', 'GFLdqBZKfPfbpbVB8rAc8tqqWSKpKHskkGHPGAgQ4atRkJ7')
    .option('--auction-id <id>', 'auction id to bid', '73')
    .option('--first-period <period>', 'first lease period to bid', '28')
    .option('--last-period <period>', 'last lease period to bid', '35')
    .option('--para-id <para-id>', 'para id', '2264')
    .option('--dry-run', 'dry run')
    .action(() =>
        main()
            .then(process.exit)
            .catch(console.error)
            .finally(() => process.exit(-1))
    )
    .parse(process.argv);

function logWinner(paraId, amount) {
    Logger.info(`Winner: ${paraId} ${amount.toString()}`);
}

async function sleep(t) {
    await new Promise(resolve => {
        setTimeout(resolve, t);
    });
}

async function main() {
    const opts = program.opts();
    const fundingAccount = opts.funding;
    const [floor, ceil, margin, intervalMs, paraId, auctionId, firstPeriod, lastPeriod] = [
        opts.floor, opts.ceil, opts.margin, opts.interval, opts.paraId,
        opts.auctionId, opts.firstPeriod, opts.lastPeriod
    ].map(x => parseInt(x));
    const { dryRun } = opts;
    const [floorBn, ceilBn, marginBn] = [floor, ceil, margin].map(x => new BN(x).mul(bn1e12));
    const privkey = process.env.PRIVKEY || '//Alice';

    Logger.info('Options', {
        fundingAccount,
        floor, ceil, margin, intervalMs, paraId, auctionId, firstPeriod, lastPeriod, dryRun,
    });

    const wsProvider = new WsProvider('wss://kusama-rpc.polkadot.io');
    const api = await ApiPromise.create({ provider: wsProvider });

    const keyring = new Keyring({ type: 'sr25519' });
    const pair = keyring.addFromUri(privkey);

    let info = await api.query.auctions.auctionInfo();
    info = info.unwrap();
    const endingPeriodStart = info[1].toNumber();
    const endingPeriod = (await api.consts.auctions.endingPeriod).toNumber();  // 72,000
    const sampleLength = (await api.consts.auctions.sampleLength).toNumber();  // 20

    const currentAuction = (await api.query.auctions.auctionCounter()).toNumber();
    if (currentAuction != auctionId) {
        Logger.info(`Not the current auction: ${auctionId} != ${currentAuction}`);
    }

    Logger.info('Constants', {endingPeriod, sampleLength});

    while (true) {
        const header = await api.rpc.chain.getHeader();
        const blocknum = header.number.toNumber();
        Logger.info(blocknum);

        let elapsed = blocknum - endingPeriodStart;
        if (elapsed < 0) {
            Logger.warn('Auction not started');
            elapsed = 0;
        } else if (elapsed > endingPeriod) {
            Logger.warn('Auction ended');
            process.exit();
        }

        const sample = (elapsed / sampleLength) | 0;
        // const _subSample = elapsed - sampleLength * sample;

        let winning = await api.query.auctions.winning(sample);
        if (winning.isSome) {
            winning = winning.unwrap();
            const winner = winning
                .filter(x => x.isSome)
                .map(x => x.unwrap())
                .reduce((best, cur) => cur[2].gt(best[2]) ? cur : best, [null, null, bn0]);

            let [winningAccountId, winningParaId, winningAmount] = winner;
            winnerParaId = winningParaId.toNumber();
            logWinner(winningParaId, winningAmount);

            if (winningParaId == paraId) {
                Logger.info('We are winning');
            } else {
                const price = BN.max(
                    BN.min(
                        winningAmount.add(marginBn),
                        ceilBn
                    ),
                    floorBn
                );
                Logger.info(`Bidding at ${price.toString()}`, {
                    paraId,
                    auctionId,
                    firstPeriod,
                    lastPeriod,
                    price: price.toString(),
                });

                if (price.lt(winningAmount)) {
                    warn('Price too high. Given up.');
                    return;
                }

                if (!dryRun) {
                    const h = await api.tx.auctions.bid(
                        paraId,
                        auctionId,
                        firstPeriod,
                        lastPeriod,
                        price,
                    ).signAndSend(pair, {nonce: -1});
                    Logger.info('Bid sent at block', blocknum, h);
                } else {
                    Logger.info('(Dry-Run) Bid sent at block', blocknum);
                }
            }
        }

        await sleep(intervalMs);
    }
}
