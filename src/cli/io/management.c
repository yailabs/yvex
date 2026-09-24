/* Restricted SSH forced-command projection for device management v1. */
#define _POSIX_C_SOURCE 200809L
#include "src/cli/io/private.h"
#include <yvex/internal/core.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define MANAGEMENT_REQUEST_CAP 4096u
#define MANAGEMENT_KEY_CAP 2048u
#define MANAGEMENT_TRUST_CAP 65536u
#define MANAGEMENT_TRUST_HEADER "# yvex.management.authorized-keys.v1\n"

static int management_base64_digit(unsigned char byte)
{
    if (byte >= 'A' && byte <= 'Z') return byte - 'A';
    if (byte >= 'a' && byte <= 'z') return byte - 'a' + 26;
    if (byte >= '0' && byte <= '9') return byte - '0' + 52;
    if (byte == '+') return 62;
    if (byte == '/') return 63;
    return -1;
}

static int management_key_decode(const char *encoded, size_t count,
                                 unsigned char *output, size_t capacity,
                                 size_t *used)
{
    size_t index, length = 0u;
    unsigned int bits = 0u, buffer = 0u;
    if (!count || count % 4u) return 0;
    for (index = 0u; index < count; ++index) {
        int digit;
        if (encoded[index] == '=') {
            size_t padding = count - index;
            if (padding > 2u || (padding == 1u && bits != 2u) ||
                (padding == 2u && bits != 4u)) return 0;
            break;
        }
        digit = management_base64_digit((unsigned char)encoded[index]);
        if (digit < 0) return 0;
        buffer = (buffer << 6u) | (unsigned int)digit;
        bits += 6u;
        if (bits >= 8u) {
            bits -= 8u;
            if (length >= capacity) return 0;
            output[length++] = (unsigned char)(buffer >> bits);
            buffer &= (1u << bits) - 1u;
        }
    }
    if (bits && buffer) return 0;
    *used = length;
    return 1;
}

static unsigned int management_u32(const unsigned char *bytes)
{
    return ((unsigned int)bytes[0] << 24u) |
           ((unsigned int)bytes[1] << 16u) |
           ((unsigned int)bytes[2] << 8u) | bytes[3];
}

static int management_encoded_identity(const char *encoded, size_t encoded_length,
                                       char identity[YVEX_SHA256_HEX_BYTES])
{
    unsigned char blob[MANAGEMENT_KEY_CAP], digest[YVEX_SHA256_DIGEST_BYTES];
    size_t used = 0u;
    yvex_sha256 hash;
    if (!management_key_decode(encoded, encoded_length, blob,
                               sizeof(blob), &used) ||
        used != 51u || management_u32(blob) != 11u ||
        memcmp(blob + 4u, "ssh-ed25519", 11u) ||
        management_u32(blob + 15u) != 32u) return 0;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update(&hash, blob, used) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, identity);
    return 1;
}

static int management_key_identity(const char *path,
                                   char identity[YVEX_SHA256_HEX_BYTES],
                                   char *encoded, size_t encoded_capacity)
{
    char *text, *cursor, *end;
    unsigned char *snapshot = NULL;
    size_t length = 0u;
    yvex_core_file_result result;
    yvex_error err = {0};
    if (!path || yvex_core_file_read_snapshot(path, MANAGEMENT_KEY_CAP,
                                              &snapshot, &length, &result,
                                              &err) != YVEX_OK) return 0;
    text = (char *)snapshot;
    if (length <= 12u || memchr(text, '\0', length) != NULL ||
        memchr(text, '\n', length - 1u) != NULL ||
        strncmp(text, "ssh-ed25519 ", 12u)) {
        free(text);
        return 0;
    }
    cursor = text + 12u;
    end = cursor;
    while (*end && !isspace((unsigned char)*end)) end++;
    if (!management_encoded_identity(cursor, (size_t)(end - cursor), identity)) {
        free(text);
        return 0;
    }
    if (encoded) {
        size_t count = (size_t)(end - cursor);
        if (count >= encoded_capacity) {
            free(text);
            return 0;
        }
        memcpy(encoded, cursor, count);
        encoded[count] = '\0';
    }
    free(text);
    return 1;
}

typedef struct {
    char request_id[65];
    char operation[48];
    int schema, id, op;
} management_request;

static int management_request_parse(const char *line, size_t length,
                                    management_request *request)
{
    yvex_json json;
    yvex_json_iter iter;
    yvex_json_item item;
    char key[48];
    memset(request, 0, sizeof(*request));
    yvex_json_init(&json, line, length);
    if (!yvex_json_iter_begin(&json, &iter, YVEX_JSON_COLLECTION_OBJECT)) return 0;
    while ((item = yvex_json_object_member(&iter, key, sizeof(key))) ==
           YVEX_JSON_ITEM_READY) {
        if (!strcmp(key, "schema") && !request->schema) {
            char schema[48];
            if (!yvex_json_string(&json, schema, sizeof(schema)) ||
                strcmp(schema, "yvex.management.request.v1")) return 0;
            request->schema = 1;
        } else if (!strcmp(key, "request_id") && !request->id) {
            if (!yvex_json_string(&json, request->request_id,
                                  sizeof(request->request_id)) ||
                strlen(request->request_id) != 64u ||
                !yvex_sha256_hex_valid(request->request_id)) return 0;
            request->id = 1;
        } else if (!strcmp(key, "operation") && !request->op) {
            if (!yvex_json_string(&json, request->operation,
                                  sizeof(request->operation))) return 0;
            request->op = 1;
        } else return 0;
    }
    return item == YVEX_JSON_ITEM_END && request->schema && request->id &&
           request->op && yvex_json_complete(&json);
}

static int management_host_summary(yvex_server_summary *summary)
{
    char socket[YVEX_SERVER_SOCKET_PATH_CAP];
    yvex_client_request request;
    yvex_client_message message;
    yvex_client *client = NULL;
    yvex_error err = {0};
    int rc;
    if (yvex_server_socket_path(socket, &err) != YVEX_OK) return -1;
    yvex_cli_client_request_init(&request, YVEX_CLIENT_OP_RUNTIME_STATUS);
    rc = yvex_client_connect(&client, socket, &err);
    if (rc != YVEX_OK) {
        struct stat status;
        return lstat(socket, &status) == 0 ? -1 : 0;
    }
    rc = yvex_client_timeout_set(client, 2000ull, &err);
    if (rc == YVEX_OK) rc = yvex_client_send(client, &request, &err);
    if (rc == YVEX_OK) rc = yvex_client_receive(client, &message, &err);
    if (rc == YVEX_OK && message.kind == YVEX_CLIENT_MESSAGE_STATUS)
        *summary = message.runtime;
    else rc = YVEX_ERR_STATE;
    yvex_client_close(&client);
    return rc == YVEX_OK ? 1 : -1;
}

static void management_response(const management_request *request,
                                const char *device, const char *peer)
{
    if (!strcmp(request->operation, "device.describe")) {
        printf("{\"schema\":\"yvex.management.response.v1\","
               "\"request_id\":\"%s\",\"status\":\"ok\","
               "\"device_identity\":\"ssh-ed25519:sha256:%s\","
               "\"authenticated_peer\":\"ssh-ed25519:sha256:%s\"}\n",
               request->request_id, device, peer);
    } else if (!strcmp(request->operation, "host.status")) {
        yvex_server_summary summary;
        int state = management_host_summary(&summary);
        if (state == 1)
            printf("{\"schema\":\"yvex.management.response.v1\","
                   "\"request_id\":\"%s\",\"status\":\"ok\","
                   "\"device_identity\":\"ssh-ed25519:sha256:%s\","
                   "\"authenticated_peer\":\"ssh-ed25519:sha256:%s\","
                   "\"host_state\":\"running\",\"engine_count\":%llu,"
                   "\"loaded_engine_count\":%llu,\"session_count\":%llu}\n",
                   request->request_id, device, peer, summary.engine_count,
                   summary.loaded_engine_count, summary.session_count);
        else
            printf("{\"schema\":\"yvex.management.response.v1\","
                   "\"request_id\":\"%s\",\"status\":\"ok\","
                   "\"device_identity\":\"ssh-ed25519:sha256:%s\","
                   "\"authenticated_peer\":\"ssh-ed25519:sha256:%s\","
                   "\"host_state\":\"%s\"}\n",
                   request->request_id, device, peer,
                   state == 0 ? "stopped" : "unavailable");
    } else
        printf("{\"schema\":\"yvex.management.response.v1\","
               "\"request_id\":\"%s\",\"status\":\"refused\","
               "\"device_identity\":\"ssh-ed25519:sha256:%s\","
               "\"authenticated_peer\":\"ssh-ed25519:sha256:%s\","
               "\"reason\":\"unsupported_operation\"}\n",
               request->request_id, device, peer);
}

static int management_safe_command_path(const char *path)
{
    const unsigned char *cursor = (const unsigned char *)path;
    if (!cursor || *cursor != '/') return 0;
    for (; *cursor; ++cursor)
        if (!isalnum(*cursor) && *cursor != '/' && *cursor != '_' &&
            *cursor != '-' && *cursor != '.') return 0;
    return 1;
}

static int management_trust_parent_safe(const char *path)
{
    char directory[PATH_MAX];
    char *slash;
    struct stat status;
    if (!path || strlen(path) >= sizeof(directory) || path[0] != '/') return 0;
    memcpy(directory, path, strlen(path) + 1u);
    slash = strrchr(directory, '/');
    if (!slash || !slash[1]) return 0;
    if (slash == directory) slash[1] = '\0';
    else *slash = '\0';
    return lstat(directory, &status) == 0 && S_ISDIR(status.st_mode) &&
           status.st_uid == geteuid() && (status.st_mode & 022u) == 0u;
}

static int management_trust_file(const char *path, char **text, size_t *length)
{
    yvex_core_file_result result;
    yvex_error err = {0};
    unsigned char *data = NULL;
    struct stat status;
    if (!path || lstat(path, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_uid != geteuid() || (status.st_mode & 077u) != 0u ||
        yvex_core_file_read_snapshot(path, MANAGEMENT_TRUST_CAP, &data,
                                     length, &result, &err) != YVEX_OK) return 0;
    if (*length < sizeof(MANAGEMENT_TRUST_HEADER) - 1u ||
        memcmp(data, MANAGEMENT_TRUST_HEADER,
               sizeof(MANAGEMENT_TRUST_HEADER) - 1u) ||
        memchr(data, '\0', *length) != NULL) {
        free(data);
        return 0;
    }
    *text = (char *)data;
    return 1;
}

static int management_trust_line_id(const char *line, size_t length,
                                    char identity[YVEX_SHA256_HEX_BYTES])
{
    static const char marker[] = " yvex-management:";
    static const char key_prefix[] = "restrict,command=\"";
    static const char key_separator[] = "\" ssh-ed25519 ";
    static const char verb[] = " management protocol ";
    const char *suffix, *closing, *key, *operation, *host, *peer, *trust;
    char command[4096], digest[YVEX_SHA256_HEX_BYTES];
    size_t prefix = sizeof(marker) - 1u, command_length;
    if (length < prefix + 65u || line[length - 1u] != '\n' ||
        length >= sizeof(command) || length < sizeof(key_prefix) - 1u ||
        memcmp(line, key_prefix, sizeof(key_prefix) - 1u)) return 0;
    suffix = line + length - prefix - 65u;
    if (suffix - line <= (ptrdiff_t)sizeof(key_prefix) - 1 ||
        memcmp(suffix, marker, prefix)) return 0;
    memcpy(identity, suffix + prefix, 64u);
    identity[64] = '\0';
    if (!yvex_sha256_hex_valid(identity)) return 0;
    closing = memchr(line + sizeof(key_prefix) - 1u, '"',
                     (size_t)(suffix - line) - (sizeof(key_prefix) - 1u));
    if (!closing || suffix - closing < (ptrdiff_t)sizeof(key_separator) - 1 ||
        memcmp(closing, key_separator, sizeof(key_separator) - 1u)) return 0;
    key = closing + sizeof(key_separator) - 1u;
    if (!management_encoded_identity(key, (size_t)(suffix - key), digest) ||
        strcmp(digest, identity)) return 0;
    command_length = (size_t)(closing - (line + sizeof(key_prefix) - 1u));
    memcpy(command, line + sizeof(key_prefix) - 1u, command_length);
    command[command_length] = '\0';
    operation = strstr(command, verb);
    if (!operation) return 0;
    command[operation - command] = '\0';
    if (!management_safe_command_path(command)) return 0;
    host = operation + sizeof(verb) - 1u;
    peer = strchr(host, ' ');
    if (!peer) return 0;
    command[peer - command] = '\0';
    if (!management_safe_command_path(host)) return 0;
    trust = strchr(peer + 1u, ' ');
    if (!trust) return 0;
    command[trust - command] = '\0';
    return !strcmp(peer + 1u, identity) &&
           management_safe_command_path(trust + 1u);
}

static int management_trust_find(const char *text, size_t length, const char *peer,
                                 size_t *offset, size_t *line_length)
{
    size_t cursor = sizeof(MANAGEMENT_TRUST_HEADER) - 1u;
    int found = 0;
    while (cursor < length) {
        const char *end = memchr(text + cursor, '\n', length - cursor);
        char identity[YVEX_SHA256_HEX_BYTES];
        size_t extent;
        if (!end) return -1;
        extent = (size_t)(end - (text + cursor)) + 1u;
        if (!management_trust_line_id(text + cursor, extent, identity)) return -1;
        if (!strcmp(identity, peer)) {
            if (found) return -1;
            *offset = cursor;
            *line_length = extent;
            found = 1;
        }
        cursor += extent;
    }
    return found;
}

static int management_peer_still_enrolled(const char *path, const char *peer)
{
    char *text = NULL;
    size_t length = 0u, offset = 0u, extent = 0u;
    int found;
    if (!management_safe_command_path(path) ||
        !management_trust_file(path, &text, &length)) return 0;
    found = management_trust_find(text, length, peer, &offset, &extent);
    free(text);
    return found == 1;
}

static int management_trust_lock(const char *path)
{
    char lock_path[PATH_MAX];
    struct stat status;
    int fd;
    if (!management_trust_parent_safe(path) ||
        snprintf(lock_path, sizeof(lock_path), "%s.lock", path) >=
                     (int)sizeof(lock_path)) return -1;
    fd = open(lock_path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return -1;
    if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_uid != geteuid() || (status.st_mode & 077u) != 0u ||
        flock(fd, LOCK_EX) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int management_trust_publish(const char *path, const char *text, size_t length)
{
    yvex_core_file_result result;
    yvex_error err = {0};
    return yvex_core_file_publish_replace(path, text, length, NULL, NULL,
                                          &result, &err) == YVEX_OK;
}

static int management_local_refuse(const char *reason)
{
    fprintf(stderr, "yvex management: refused: %s\n", reason);
    return 2;
}

static int management_trust_init(const char *path)
{
    yvex_core_file_result result;
    yvex_error err = {0};
    if (!management_trust_parent_safe(path))
        return management_local_refuse("unsafe_trust_parent");
    if (yvex_core_file_publish_noreplace(
        path, MANAGEMENT_TRUST_HEADER, sizeof(MANAGEMENT_TRUST_HEADER) - 1u,
        NULL, NULL, NULL, &result, &err) != YVEX_OK)
        return management_local_refuse("trust_file_exists_or_unavailable");
    return 0;
}

static int management_trust_change(int argc, char **argv, int enroll)
{
    char peer[YVEX_SHA256_HEX_BYTES], host[YVEX_SHA256_HEX_BYTES];
    char encoded[MANAGEMENT_KEY_CAP], executable[PATH_MAX], line[4096];
    char *text = NULL, *updated = NULL;
    const char *path;
    size_t length = 0u, offset = 0u, extent = 0u, added = 0u;
    int lock = -1, found, status = 2;
    const char *refusal = "trust_publication_failed";
    if ((enroll && argc != 6) || (!enroll && argc != 4))
        return management_local_refuse("malformed_arguments");
    path = argv[3];
    if (enroll) {
        ssize_t executable_length;
        if (!management_key_identity(argv[2], peer, encoded, sizeof(encoded)) ||
            strcmp(peer, argv[5]) ||
            !management_key_identity(argv[4], host, NULL, 0u) ||
            !management_safe_command_path(argv[4]) ||
            !management_safe_command_path(path))
            return management_local_refuse("peer_or_host_identity_mismatch");
        executable_length = readlink("/proc/self/exe", executable,
                                     sizeof(executable) - 1u);
        if (executable_length <= 0 || executable_length >= (ssize_t)sizeof(executable))
            return management_local_refuse("executable_identity_unavailable");
        executable[executable_length] = '\0';
        if (!management_safe_command_path(executable))
            return management_local_refuse("unsafe_executable_path");
        added = (size_t)snprintf(line, sizeof(line),
            "restrict,command=\"%s management protocol %s %s %s\" "
            "ssh-ed25519 %s yvex-management:%s\n",
            executable, argv[4], peer, path, encoded, peer);
        if (added >= sizeof(line)) return management_local_refuse("entry_too_large");
    } else if (!yvex_sha256_hex_valid(argv[2]))
        return management_local_refuse("malformed_peer_identity");
    else memcpy(peer, argv[2], sizeof(peer));
    lock = management_trust_lock(path);
    if (lock < 0 || !management_trust_file(path, &text, &length)) {
        refusal = "trust_file_unavailable_or_malformed";
        goto done;
    }
    found = management_trust_find(text, length, peer, &offset, &extent);
    if (found < 0) {
        refusal = "trust_file_malformed";
        goto done;
    }
    if (enroll && found) {
        refusal = "peer_already_enrolled";
        goto done;
    }
    if (!enroll && !found) {
        refusal = "peer_not_enrolled";
        goto done;
    }
    if (enroll && length + added > MANAGEMENT_TRUST_CAP) {
        refusal = "trust_file_full";
        goto done;
    }
    updated = malloc(length + added + 1u);
    if (!updated) {
        refusal = "allocation_failed";
        goto done;
    }
    if (enroll) {
        memcpy(updated, text, length);
        memcpy(updated + length, line, added);
        length += added;
    } else {
        memcpy(updated, text, offset);
        memcpy(updated + offset, text + offset + extent, length - offset - extent);
        length -= extent;
    }
    if (management_trust_publish(path, updated, length)) {
        printf("%s ssh-ed25519:sha256:%s\n", enroll ? "enrolled" : "revoked", peer);
        status = 0;
    }
done:
    free(text);
    free(updated);
    if (lock >= 0) close(lock);
    if (status != 0) return management_local_refuse(refusal);
    return status;
}

int yvex_cli_management_command(int argc, char **argv)
{
    char device[YVEX_SHA256_HEX_BYTES], line[MANAGEMENT_REQUEST_CAP];
    management_request request;
    const char *connection = getenv("SSH_CONNECTION");
    const char *original = getenv("SSH_ORIGINAL_COMMAND");
    const char *tty = getenv("SSH_TTY");
    size_t length;
    if (argc == 3 && !strcmp(argv[1], "identity")) {
        if (!management_key_identity(argv[2], device, NULL, 0u)) return 2;
        printf("ssh-ed25519:sha256:%s\n", device);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "trust-init"))
        return management_trust_init(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "enroll"))
        return management_trust_change(argc, argv, 1);
    if (argc >= 2 && !strcmp(argv[1], "revoke"))
        return management_trust_change(argc, argv, 0);
    if (argc != 5 || strcmp(argv[1], "protocol") || !connection || !*connection ||
        (original && *original) || (tty && *tty) ||
        !yvex_sha256_hex_valid(argv[3]) ||
        !management_key_identity(argv[2], device, NULL, 0u)) return 2;
    if (!management_peer_still_enrolled(argv[4], argv[3])) {
        printf("{\"schema\":\"yvex.management.response.v1\","
               "\"request_id\":null,\"status\":\"refused\","
               "\"reason\":\"peer_revoked_or_authority_unavailable\"}\n");
        return fflush(stdout) == 0 ? 0 : 1;
    }
    if (!fgets(line, sizeof(line), stdin)) return 2;
    length = strlen(line);
    if (!length || line[length - 1u] != '\n' ||
        !management_request_parse(line, length, &request)) {
        printf("{\"schema\":\"yvex.management.response.v1\","
               "\"request_id\":null,\"status\":\"refused\","
               "\"reason\":\"malformed_request\"}\n");
        return fflush(stdout) == 0 ? 0 : 1;
    }
    management_response(&request, device, argv[3]);
    return fflush(stdout) == 0 ? 0 : 1;
}
