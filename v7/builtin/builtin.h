/*
 * Copyright (c) 2015 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Non-Standard API
 *
 *   V7 has several non-standard extensions for `String.prototype` in
 *   order to give a compact and fast API to access raw data obtained from
 *   File, Socket, and hardware input/output such as I2C.
 *   V7 IO API functions return
 *   string data as a result of read operations, and that string data is a
 *   raw byte array. ECMA6 provides `ArrayBuffer` and `DataView` API for dealing
 *   with raw bytes, because strings in JavaScript are Unicode. That standard
 *   API is too bloated for the embedded use, and does not allow to use handy
 *   String API (e.g. `.match()`) against data.
 *
 *   V7 internally stores strings as byte arrays. All strings created by the
 *   String API are UTF8 encoded. Strings that are the result of
 *   input/output API calls might not be a valid UTF8 strings, but nevertheless
 *   they are represented as strings, and the following API allows to access
 *   underlying byte sequence:
 *
 * ==== String.prototype.at(position) -> number or NaN
 *      Return byte at index
 *     `position`. Byte value is in 0,255 range. If `position` is out of bounds
 *     (either negative or larger then the byte array length), NaN is returned.
 *     Example: `"ы".at(0)` returns 0xd1.
 *
 * ==== String.prototype.blen -> number
 *     Return string length in bytes.
 *     Example: `"ы".blen` returns 2. Note that `"ы".length` is 1, since that
 *     string consists of a single Unicode character (2-byte).
 *
 * === Builtin API
 *
 * Builtin API provides additional JavaScript interfaces available for V7
 * scripts.
 * File API is a wrapper around standard C calls `fopen()`, `fclose()`,
 * `fread()`, `fwrite()`, `rename()`, `remove()`.
 * Crypto API provides functions for base64, md5, and sha1 encoding/decoding.
 * Socket API provides low-level socket API.
 *
 * ==== File.eval(file_name)
 * Parse and run `file_name`.
 * Throws an exception if the file doesn't exist, cannot be parsed or if the
 * script throws any exception.
 *
 * ==== File.read(file_name) -> string or undefined
 * Read file `file_name` and return a string with a file content.
 * On any error, return `undefined` as a result.
 *
 * ==== File.write(file_name, str) -> true or false
 * Write string `str` to a file `file_name`. Return `true` on success,
 * `false` on error.
 *
 * ==== File.open(file_name [, mode]) -> file_object or null
 * Open a file `path`. For
 * list of valid `mode` values, see `fopen()` documentation. If `mode` is
 * not specified, mode `rb` is used, i.e. file is opened in read-only mode.
 * Return an opened file object, or null on error. Example:
 * `var f = File.open('/etc/passwd'); f.close();`
 *
 * ==== file_obj.close() -> undefined
 * Close opened file object.
 * NOTE: it is user's responsibility to close all opened file streams. V7
 * does not do that automatically.
 *
 * ==== file_obj.read() -> string
 * Read portion of data from
 * an opened file stream. Return string with data, or empty string on EOF
 * or error.
 *
 * ==== file_obj.write(str) -> num_bytes_written
 * Write string `str` to the opened file object. Return number of bytes written.
 *
 * ==== File.rename(old_name, new_name) -> errno
 * Rename file `old_name` to
 * `new_name`. Return 0 on success, or `errno` value on error.
 *
 * ==== File.list(dir_name) -> array_of_names
 * Return a list of files in a given directory, or `undefined` on error.
 *
 * ==== File.remove(file_name) -> errno
 * Delete file `file_name`.
 * Return 0 on success, or `errno` value on error.
 *
 * ==== Crypto.base64_encode(str)
 * Base64-encode input string `str` and return encoded string.
 *
 * ==== Crypto.base64_decode(str)
 * Base64-decode input string `str` and return decoded string.
 *
 * ==== Crypto.md5(str), Crypto.md5_hex(str)
 * Generate MD5 hash from input string `str`. Return 16-byte hash (`md5()`),
 * or stringified hexadecimal representation of the hash (`md5_hex`).
 *
 * ==== Crypto.sha1(str), Crypto.sha1_hex(str)
 * Generate SHA1 hash from input string `str`. Return 20-byte hash (`sha1()`),
 * or stringified hexadecimal representation of the hash (`sha1_hex`).
 *
 * ==== Socket.connect(host, port [, is_udp]) -> socket_obj
 * Connect to a given host. `host` can be a string IP address, or a host name.
 * Optional `is_udp` parameter, if true, indicates that socket should be UDP.
 * Return socket object on success, null on error.
 *
 * ==== Socket.listen(port [, ip_address [,is_udp]]) -> socket_obj
 * Create a listening socket on a given port. Optional `ip_address` argument
 * specifies and IP address to bind to. Optional `is_udp` parameter, if true,
 * indicates that socket should be UDP. Return socket object on success,
 * null on error.
 *
 * ==== socket_obj.accept() -> socket_obj
 * Sleep until new incoming connection is arrived. Return accepted socket
 * object on success, or `null` on error.
 *
 * ==== socket_obj.close() -> numeric_errno
 * Close socket object. Return 0 on success, or system errno on error.
 *
 * ==== socket_obj.recv() -> string
 * Read data from socket. Return data string, or empty string if peer has
 * disconnected, or `null` on error.
 *
 * ==== socket_obj.recvAll() -> string
 * Same as `recv()`, but keeps reading data until socket is closed.
 *
 * ==== sock.send(string) -> num_bytes_sent
 * Send string to the socket. Return number of bytes sent, or 0 on error.
 * Simple HTTP client example:
 *
 *    var s = Socket.connect("google.com", 80);
 *    s.send("GET / HTTP/1.0\n\n");
 *    var reply = s.recv();
 */

#ifndef CS_V7_BUILTIN_BUILTIN_H_
#define CS_V7_BUILTIN_BUILTIN_H_

struct v7;

void init_file(struct v7 *);
void init_socket(struct v7 *);
void init_crypto(struct v7 *);
void init_ubjson(struct v7 *);
void init_ubjson(struct v7 *v7);

#endif /* CS_V7_BUILTIN_BUILTIN_H_ */
