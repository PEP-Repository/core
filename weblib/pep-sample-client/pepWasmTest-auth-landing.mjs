export {}

(() => {
    const messageElem = document.getElementById('message');

    const params = new URLSearchParams(location.search);
    const redirect = params.get('redirect');
    if (redirect) {
        location.assign(redirect);
        return;
    }

    const chanName = params.get('chan');
    if (!chanName) {
        messageElem.textContent = 'Could not find window to log in to.';
        document.documentElement.classList.add('failure');
        return;
    }
    const chan = new BroadcastChannel(chanName);

    const error = params.get('error');
    if (error) {
        let errorMsg = error;
        const errorDescription = params.get('error_description');
        if (errorDescription) {
            errorMsg = `${errorDescription} (${error})`;
        }
        messageElem.textContent = errorMsg;
        document.documentElement.classList.add('failure');
        chan.postMessage({error, errorDescription});
        return;
    }

    const code = params.get('code');
    if (!code) {
        messageElem.textContent = 'No authentication code received.';
        document.documentElement.classList.add('failure');
        return;
    }

    chan.postMessage({code});
    messageElem.textContent = 'You have been logged in.';
    close();
})();
