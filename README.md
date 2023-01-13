# WARNING: This project is defunct and archived.

# Nano PoW Server
[![Build Status](https://travis-ci.org/nanocurrency/nano-pow-server.svg)](https://travis-ci.org/nanocurrency/nano-pow-server)

This project is a standalone work server for [nano-pow](https://github.com/nanocurrency/nano-pow), the proof-of-work algorithm used by Nano.

## Download

* [Linux](s3://repo.nano.org/pow-server/nano_pow_server-latest-Linux.tar.gz)
* [Mac](s3://repo.nano.org/pow-server/nano_pow_server-latest-Darwin.tar.gz)
* [Windows](s3://repo.nano.org/pow-server/nano_pow_server-latest-win64.tar.gz)

## Installation

CMake is used for building. C++14 and Boost 1.69 or later is required. Other dependencies are managed with git submodules:

```
git clone --recursive https://github.com/nanocurrency/nano-pow-server.git
cd nano-pow-server; mkdir build; cd build
cmake -DNANO_POW_STANDALONE=ON ..
make
```

On Windows, some flags may need to be passed to cmake:

`-Dgtest_force_shared_crt=on` when building tests

`-A "x64"` to pick the correct Boost libraries

### Validate installation

```
./nano_pow_server --config server.log_to_stderr=true
```

```
curl localhost:8076/api/v1/ping
{"success": "true"}
```

## Configuration

Defaults can be overriden by using a TOML config file, or by passing options through the `--config` command line option.

**Note:** One or more CPU or GPU work devices *must* be configured.

#### Configuration file

To override defaults using a config file, create a file called `nano-pow-server.toml` and add the required keys and values under their respective TOML table. The file name can optionally be specified with the `--config_path` option (if not specified, the working directory is searched)

**A documented configuration file can be created with the --generate_config command**

**A documented configuration file is created in the build root when the nano_pow_server target is created**

**A documented configuration file is created with the `package` target**

While configuring devices is more convenient in the TOML file, a single device can be configured via the command line using something like `--config device.type=\"cpu\" --config device.threads=4`

Multiple devices can be added by placing several `[[device]]` entries in the config file.

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

This can be used by clients and tools to check if the work server is available.

*URL* : `/api/v1/ping`

*Method* : `GET`

##### Response

```json

{
  "success": "true",
}
```

### Version

This can be used by clients and tools to receive the server version number.

*URL* : `/api/v1/version`

*Method* : `GET`

##### Response

```json

{
  "version": "1.0.0",
}
```

### Queue
*Experimental: This endpoint may change or be removed in future versions without further notice*

This can be used by clients and tools to display the current work queue (pending, in progress and completed)

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
  "success": "true",
}
```

An error is returned if control requests are not allowed.

### Stop

This can be used by clients and tools to stop the work server process.

This request requires `server.allow_control` to be set to true in the config.

*URL* : `/api/v1/stop`

*Method* : `GET`

##### Response

```json

{
  "success": "true",
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

A sample reverse proxy setup is available here:

https://gist.github.com/cryptocode/c7d63216902615a06fea6e5c948960d2
