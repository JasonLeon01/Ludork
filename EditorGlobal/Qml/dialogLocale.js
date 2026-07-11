.pragma library

function t(key, host) {
    if (host === undefined || host === null)
        return "";
    return host.localize(key);
}
