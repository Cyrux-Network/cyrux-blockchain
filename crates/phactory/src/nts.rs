use anyhow::{Context, Result};

use cyrux_nts::{get_time, NtpResult};

/// Ref: https://github.com/jauderho/nts-servers/
const TRUSTED_NTS_SERVERS: &[&str] = &[
    "time.cloudflare.com",
    "gps.ntp.br",
    "a.st1.ntp.br",
    "paris.time.system76.com",
    "ntp3.fau.de",
    "ptbtime1.ptb.de",
    "ntppool1.time.nl",
    "nts.netnod.se",
];

/// Get time from NTS servers
pub(crate) async fn nts_get_time_secs(servers: &[String]) -> Result<u64> {
    const TIMEOUT_SECS: u64 = 5;
    let mut futures = Vec::new();
    if servers.is_empty() {
        for server in TRUSTED_NTS_SERVERS.iter() {
            futures.push(get_time_timeout(server, TIMEOUT_SECS));
        }
    } else {
        for server in servers.iter() {
            futures.push(get_time_timeout(server, TIMEOUT_SECS));
        }
    }
    info!("Requesting time from {} servers", futures.len());
    let results = futures::future::join_all(futures)
        .await
        .into_iter()
        .filter_map(|r| r.ok())
        .map(|r| r.receive_time_duration().as_secs())
        .collect::<Vec<_>>();
    info!("Got time from {} servers", results.len());
    validate_results(results).context("Failed to get time from NTS servers")
}

fn validate_results(results: Vec<u64>) -> Result<u64> {
    const MIN_RESULTS: usize = 2;
    const MAX_VARIANCE: u64 = 60;
    if results.len() < MIN_RESULTS {
        anyhow::bail!("Not enough results");
    }
    let average = results.iter().sum::<u64>() / results.len() as u64;
    let max_diff = results
        .iter()
        .map(|r| (*r as i64 - average as i64).unsigned_abs())
        .max()
        .unwrap_or_default();
    if max_diff > MAX_VARIANCE {
        anyhow::bail!("Time difference is too large: {}", max_diff);
    }
    Ok(average)
}

async fn get_time_timeout(server: &str, timeout: u64) -> Result<NtpResult> {
    let timeout = std::time::Duration::from_secs(timeout);
    tokio::time::timeout(timeout, get_time(server, None)).await?
}
