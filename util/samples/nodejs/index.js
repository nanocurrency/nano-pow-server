// Get the ws[s]:// target from the command line, use a default is missing
const args = process.argv.slice(2);
const default_target = 'ws://[::1]:8076/websocket';
const target = args.length > 0 ? args[0] : default_target;

if (args.length == 0) {
	console.log('Target not specified, connecting to ' + default_target);
}

const WS = require('ws');
const ReconnectingWebSocket = require('reconnecting-websocket');

// Create a reconnecting WebSocket.
// In this example, we wait a maximum of 2 seconds before retrying.
const ws = new ReconnectingWebSocket(target, [], {
	WebSocket: WS,
	connectionTimeout: 1000,
	maxRetries: 100000,
	maxReconnectionDelay: 2000,
	minReconnectionDelay: 10
});

// As soon as we connect, send a work request
ws.onopen = () => {
	console.log('Connected successfully, sending work request. Awaiting generated work...');
	const confirmation_subscription = {
		"action": "work_generate",
		"hash": "718CC2121C3E641059BC1C2CFC45666C99E8AE922F7A807B7D07B62C995D79E2",
		"difficulty": "2000000000000000",
		"multiplier": "1.0",
		"id": "73018"
	}
	ws.send(JSON.stringify(confirmation_subscription));
};

ws.onerror = (error) => {
  console.error("WebSocket error:", error.message);
};

// The work server sent us a message
ws.onmessage = msg => {
	console.log(msg.data);
	data_json = JSON.parse(msg.data);
	console.log('WebSocket test completed successfully');
	process.exit(0);
};
