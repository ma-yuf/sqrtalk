const loginContainer = document.getElementById('login-container');
const chatContainer = document.getElementById('chat-container');
const loginButton = document.getElementById('login-button');
const sendButton = document.getElementById('send-button');
const usernameInput = document.getElementById('username');
const passwordInput = document.getElementById('password');
const messageInput = document.getElementById('message-input');
const chatWindow = document.getElementById('chat-window');
const loginError = document.getElementById('login-error');
const chatError = document.getElementById('chat-error');

let socket;
let lastUsername = '';
let pendingCommand = null;

loginButton.addEventListener('click', login);

usernameInput.addEventListener('keypress', (event) => {
    if (event.key === 'Enter') {
        passwordInput.focus();
    }
});

passwordInput.addEventListener('keypress', (event) => {
    if (event.key === 'Enter') {
        login();
    }
});

function login() {
    const username = usernameInput.value;
    const password = passwordInput.value;

    socket = new WebSocket('ws://154.44.8.112:9002');

    socket.onopen = () => {
        const loginData = {
            type: 'login',
            username: username,
            password: password
        };
        socket.send(JSON.stringify(loginData));
    };

    socket.onmessage = handleMessage;

    socket.onerror = (error) => {
        loginError.textContent = '连接错误，请重试。';
        setTimeout(() => {
            loginError.textContent = '';
        }, 3000);
    };

    socket.onclose = (event) => {
        chatError.textContent = '连接已关闭，请重试。';
        setTimeout(() => {
            chatError.textContent = '';
        }, 3000);
    };
}

sendButton.addEventListener('click', () => {
    const message = messageInput.value;
    const recipient = document.getElementById('private-recipient').value;
    if (message) {
        if (recipient) {
            const privateMessageData = {
                type: 'private_message',
                to: recipient,
                content: message
            };
            socket.send(JSON.stringify(privateMessageData));
            displayMessage('private_message', { from: usernameInput.value, to: recipient, content: message });
        } else if (message.startsWith('/')) {
            if (message.startsWith('/kick') || message.startsWith('/ban')) {
                pendingCommand = message;
                displayMessage('system', { content: '请输入管理员密码' });
            } else {
                const commandData = {
                    type: 'command',
                    command: message
                };
                socket.send(JSON.stringify(commandData));
            }
        } else {
            const messageData = {
                type: 'message',
                message: message
            };
            socket.send(JSON.stringify(messageData));
        }
        messageInput.value = '';
        document.getElementById('private-recipient').value = '';
    }
});

messageInput.addEventListener('keypress', (event) => {
    if (event.key === 'Enter') {
        if (pendingCommand) {
            const password = messageInput.value;
            const commandData = {
                type: 'command',
                command: pendingCommand,
                password: password
            };
            socket.send(JSON.stringify(commandData));
            messageInput.value = '';
            pendingCommand = null;
        } else {
            sendButton.click();
        }
    }
});

function handleMessage(event) {
    const response = JSON.parse(event.data);
    if (response.type === 'login') {
        if (response.status === 'success') {
            loginContainer.style.display = 'none';
            chatContainer.style.display = 'block';
            messageInput.focus();
        } else if (response.status === 'error') {
            loginError.textContent = response.message;
            setTimeout(() => {
                loginError.textContent = '';
            }, 3000);
        }
    } else if (response.type === 'message' && response.username && response.content) {
        displayMessage('message', response);
    } else if (response.type === 'private_message' && response.from && response.content) {
        displayMessage('private_message', response);
    } else if (response.type === 'user_list' && response.users) {
        displayMessage('user_list', response);
    } else if (response.status === 'error') {
        chatError.textContent = response.message;
        setTimeout(() => {
            chatError.textContent = '';
        }, 3000);
    } else if (response.type === 'command' && response.message !== 'User list sent.') {
        displayMessage('system', { content: response.message });
        pendingCommand = null;
    }
}

function displayMessage(type, data) {
    const messageElement = document.createElement('div');
    messageElement.classList.add('message', type === 'private_message' ? 'private' : (data.username === usernameInput.value ? 'user' : 'other'));
    
    if (type === 'private_message') {
        if (data.from === usernameInput.value) {
            const usernameElement = document.createElement('div');
            usernameElement.classList.add('username', 'user');
            usernameElement.textContent = `对 ${data.to} 说`;
            chatWindow.appendChild(usernameElement);
            messageElement.classList.add('user');
            lastUsername = usernameInput.value;
        } else if (data.from !== lastUsername) {
            const usernameElement = document.createElement('div');
            usernameElement.classList.add('username', 'private');
            usernameElement.textContent = `${data.from} 对我说`;
            chatWindow.appendChild(usernameElement);
            lastUsername = data.from;
        }
    } else if (type === 'user_list') {
        const usernameElement = document.createElement('div');
        usernameElement.classList.add('username', 'system');
        usernameElement.textContent = '系统';
        chatWindow.appendChild(usernameElement);
        messageElement.classList.add('system');
        messageElement.innerHTML = `<span class="content">在线用户: ${data.users.join(', ')}</span>`;
    } else if (type === 'system') {
        const usernameElement = document.createElement('div');
        usernameElement.classList.add('username', 'system');
        usernameElement.textContent = '系统';
        chatWindow.appendChild(usernameElement);
        messageElement.classList.add('system');
        messageElement.innerHTML = `<span class="content">${data.content}</span>`;
    } else {
        if (data.username !== lastUsername) {
            const usernameElement = document.createElement('div');
            usernameElement.classList.add('username', data.username === usernameInput.value ? 'user' : 'other');
            usernameElement.textContent = data.username;
            chatWindow.appendChild(usernameElement);
            lastUsername = data.username;
        }
    }
    
    if (type !== 'user_list' && type !== 'system') {
        messageElement.innerHTML = `<span class="content">${data.content}</span>`;
    }
    chatWindow.appendChild(messageElement);
    chatWindow.scrollTop = chatWindow.scrollHeight;
}

document.getElementById('toggle-private-recipient').addEventListener('click', () => {
    const privateRecipientInput = document.getElementById('private-recipient');
    if (privateRecipientInput.style.display === 'none' || privateRecipientInput.style.display === '') {
        privateRecipientInput.style.display = 'block';
    } else {
        privateRecipientInput.style.display = 'none';
    }
});