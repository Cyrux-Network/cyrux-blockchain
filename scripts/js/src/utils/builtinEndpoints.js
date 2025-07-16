module.exports = {
    resolveDefaultEndpoint(endpoint) {
        const defaultEndpoints = {
            'polkadot': 'wss://rpc.polkadot.io',
            'kusama': 'wss://kusama-rpc.polkadot.io',
            'khala': 'wss://khala-api.cyrux.network/ws',
            'cyrux': 'wss://api.cyrux.network/ws',
            'local': 'ws://localhost:9944',
        }
        if (endpoint in defaultEndpoints) {
            return defaultEndpoints[endpoint];
        }
        return endpoint;
    }
}
