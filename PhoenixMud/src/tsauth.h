/* tsauth.h  -  verify a TypeScript-platform scrypt password hash.
 *
 * The TS engine stores credentials as `salt:hash`, produced by Node's
 * crypto.scryptSync with its default parameters:
 *
 *     salt = randomBytes(16).toString('hex')      32 hex CHARACTERS
 *     hash = scryptSync(password, salt, 64)       64 bytes -> 128 hex chars
 *     stored = `${salt}:${hash}`                  161 characters
 *
 * Legacy cannot hold that today: MAX_PWD_LENGTH is 10, and the engine is built
 * NOCRYPT so it compares plaintext. This unit lets 4.2 authenticate against
 * phoenix-state directly, without a conversion step and without the
 * interchange sidecar carrying a plaintext password forward.
 *
 * Self-contained by design. 4.2 has no SHA-256, HMAC, PBKDF2 or Salsa20
 * anywhere in the tree, and the deploy host has 32-bit libcrypto but NOT the
 * 32-bit headers (opensslconf-i386.h is absent), so linking -lcrypto would
 * mean a root-level package change on production. Vendoring follows the
 * precedent already set by json.h.
 *
 * Implemented from the specifications  -  RFC 7914 (scrypt), RFC 2104 (HMAC),
 * RFC 8018 (PBKDF2) and FIPS 180-4 (SHA-256)  -  not derived from another
 * implementation.
 */

#ifndef TSAUTH_H
#define TSAUTH_H

#include <stddef.h>

/* Node's crypto.scryptSync defaults. Changing these invalidates every stored
 * hash, so they are named rather than spelled inline at the call site. */
#define TSAUTH_N       16384
#define TSAUTH_R       8
#define TSAUTH_P       1
#define TSAUTH_DKLEN   64

/* Longest `salt:hash` we accept: 32 + 1 + 128, plus a NUL. */
#define TSAUTH_HASH_LEN 161

/* Derive a key exactly as Node's scryptSync(password, salt, dklen) does.
 *
 * !! `salt` is the RAW BYTES OF THE SALT STRING. Node hands the 32-character
 * hex STRING to scrypt, so the salt is those 32 ASCII bytes  -  NOT the 16 bytes
 * they encode. Decoding the hex first produces a different key and every login
 * fails, in a way that looks exactly like a wrong password.
 *
 * Returns 0 on success, -1 on allocation failure or invalid parameters. */
int tsauth_scrypt(const unsigned char *pass, size_t passlen,
                  const unsigned char *salt, size_t saltlen,
                  unsigned long N, unsigned int r, unsigned int p,
                  unsigned char *out, size_t outlen);

/* Verify `password` against a stored TS credential of the form "salt:hash"
 * (hex). Returns 1 on match, 0 on mismatch or malformed input.
 *
 * The comparison is constant-time over the digest, so a caller cannot use
 * timing to learn how much of a hash it guessed correctly. */
int tsauth_verify(const char *password, const char *stored);

/* Does `stored` look like a TS credential rather than a legacy plaintext
 * password? Shape only: "<salt>:<128 hex chars>". Used to route the login
 * comparison, so an ordinary pfile keeps taking the plaintext path. */
int tsauth_is_ts_hash(const char *stored);

#endif /* TSAUTH_H */
