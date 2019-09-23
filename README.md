# Nano PoW Server

This project is a standalone work server for [nano-pow](https://github.com/nanocurrency/nano-pow), the proof-of-work algorithm used by Nano.

## Installation

CMake is used for building. C++14 and Boost 1.67 or later is required. Other dependencies are managed with git submodules:

```
git clone --recursive https://github.com/nanocurrency/nano-pow-server.git
cmake .
make
```

On Windows, some flag may need to be passed:

`-Dgtest_force_shared_crt=on` when building tests

`-A "x64"` to pick the correct Boost libraries

### Validate installation

```
./nano_pow_server --config server.log_to_stderr=true
```

```
curl localhost:8076/api/v1/ping
{"success": true}
```

## Configuration

Defaults can be overriden by using a TOML config file, or by passing options through the `--config` command line option.

**Note:** One or more CPU or GPU *work devices* must be configured.

#### Configuration file

Minimal  `nano-pow-server.toml` adding a single device and using default values for everything else:

```toml
[device]
type="gpu"
platform=0
device=1
threads=1048576
```

Complete `nano-pow-server.toml` with two devices:

```toml
[server]
# Listening address
bind = "0.0.0.0"

# Listening port for REST, WebSocket and the UI
port = 8076

# The maximum number of queued work requests. If the queue is full, work requests
# will result in an error.
request_limit = 16384

# If true, log to standard error in addition to file
log_to_stderr = false

# If true, work requests may contain a numeric priority property to move ahead in the queue
allow_prioritization = true

# Certain REST requests requires this to be true, otherwise an error is returned
allow_control = false

[work]
base_difficulty = "2000000000000000"

# If non-zero, the server simulates generating work for N seconds.
# instead of using a work device. Useful during testing of initial setup and debugging.
mock_work_generation_delay = 0

[[device]]
type = "cpu"
threads = 4

# Maximum allocation, in bytes
memory = 2147483648

[[device]]
type = "gpu"
platform = 0
device = 1
threads = 1048576

# Maximum allocation, in bytes
memory = 4294967296

[admin]
# If true, static web pages are available remotely, otherwise only loopback
allow_remote = false

# Path to static pages
path="public"
```

While configuring devices is more convenient in the TOML file, a single device can be configured via the command line using something like `--config device.type=\"cpu\" --config device.threads=4`

## API

The API is available as REST requests, and all the POST requests are available as through WebSockets as well, in which case clients must provide a correlation id to match up the response. The WebSocket path is `ws://localhost:8076/websocket`.

### Error responses

All requests may fail with a response containing an "error" attribute. Example:

```json
{
    "error": "Hash not found in work queue"
}
```

### Generate work

Generates proof of work given a hash and difficulty/multiplier.

*URL* : `/api/v1/work` or `/` (deprecated)

*Method* : `POST`

##### Request

Note: The `id` is only required for WebSocket requests. This can be any string.

```json
{
	"action": "work_generate",
	"hash": "718CC2121C3E641059BC1C2CFC45666C99E8AE922F7A807B7D07B62C995D79E2",
	"difficulty": "2000000000000000",
	"multiplier": "1.0",
	"id": "73018"
}
```

An optional **"priority"** attribute can be set to move the work request ahead in the queue. By default, all requests have priority 0. Note that `server.allow_prioritization` must be set to true for the priority attribute to be considered.

##### Response

```json

{
	"work": "2BF29EF00786A6BC",
	"difficulty": "201CD58F11000000",
	"multiplier": "1.394647",
	"id": "73018"
}
```

### Validate work

Validates work given a hash and difficulty/multiplier.

*URL* : `/api/v1/work` or `/` (deprecated)

*Method* : `POST`

##### Request

Note: The `id` is only required for WebSocket requests. This can be any string.

```json
{
	"action": "work_validate",
	"hash": "718CC2121C3E641059BC1C2CFC45666C99E8AE922F7A807B7D07B62C995D79E2",
	"work": "2BF29EF00786A6BC",
	"difficulty": "2000000000000000",
	"multiplier": "1.0",
	"id": "73019"
}
```

##### Response

```json

{
	"work": "2BF29EF00786A6BC",
	"difficulty": "201CD58F11000000",
	"multiplier": "1.394647",
	"id": "73019"
}
```

### Cancel work request

*URL* : `/api/v1/work` or `/` (deprecated)

*Method* : `POST`

##### Request

Note: The `id` is only required for WebSocket requests. This can be any string.

```json
{
	"action": "work_cancel",
	"hash": "718CC2121C3E641059BC1C2CFC45666C99E8AE922F7A807B7D07B62C995D79E2",
	"id": "73020"
}
```

##### Response

```json

{
	"status": "cancelled",
	"id": "73020"
}
```

An informational error response is sent if the hash is not found.

### Ping

This can be used by clients and tools, such as an web admin client, to check if the work server is available.

*URL* : `/api/v1/ping`

*Method* : `GET`

##### Response

```json

{
  "success": true,
}
```

### Queue
*Experimental: This endpoint may change or be removed in future versions without further notice*

This can be used by clients and tools, such as an web admin client, to display the current work queue (pending, in progress and completed)

*URL* : `/api/v1/work/queue`

*Method* : `GET`

##### Response

The response contains information about pending, in progress and completed work requests in json format. The detailed structure is currently not specified.

### Queue clear
*Experimental: This endpoint may change or be removed in future versions without further notice*

By sending a DELETE request to the queue endpoint, the pending queue is cleared. This does not affect work requests already being processed.

This request requires `server.allow_control` to be set to true in the config.

*URL* : `/api/v1/work/queue`

*Method* : `DELETE`

##### Response


```json

{
  "success": true,
}
```

An error is returned if control requests are not allowed.

### Stop

This can be used by clients and tools, such as an web admin client, to stop the work server process.

This request requires `server.allow_control` to be set to true in the config.

*URL* : `/api/v1/stop`

*Method* : `GET`

##### Response

```json

{
  "success": true,
}
```

An error is returned if control requests are not allowed.

## Web UI

Any administrative UI using the above REST/WebSocket features can be added by placing HTML, Javascript and other files in a subdirectory called *public* (this can be changed with the `admin.path` config setting.) By default, only loopback requests are allowed; remote access can be granted by setting the `admin.allow_remote` config option to true.

A sample UI is included in the distribution, available at http://localhost:8076

## Security

Authentication and access control is beyond the scope of the Nano PoW Server. To expose functionality, consider using a reverse proxy; this can be used to control access to the REST, WebSocket and admin interfaces.

A setup with certificates (enabling https:// and wss://) can be done fairly easy with Let's Encrypt and nginx. Here are some sample resources:

https://www.digitalocean.com/community/tutorials/how-to-secure-nginx-with-let-s-encrypt-on-ubuntu-18-04

https://www.digitalocean.com/community/tutorials/how-to-configure-nginx-with-ssl-as-a-reverse-proxy-for-jenkins
