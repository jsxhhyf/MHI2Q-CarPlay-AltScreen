/*
 * Route-guidance message registration inside the iAP2 IdentificationInformation
 * (0x1D01) frame.
 *
 * The phone only exchanges messages the accessory declared in Identify:
 * parameter 6 (MessagesSentByAccessory) and 7 (MessagesReceivedFromDevice),
 * each a plain list of big-endian uint16 message IDs.  CN dio_manager builds
 * those lists from dio_manager.json, which the installer extends with
 * 0x5200..0x5204.  ER P3638 dio_manager has no such keys, so the hook appends
 * the same IDs to the outgoing frame instead.  IDs already present are left
 * alone, so on CN the frame is byte-for-byte unchanged.
 *
 * Pure byte manipulation: no logging, no QNX, testable on the host.
 */
#ifndef RGD_IDENTIFY_H
#define RGD_IDENTIFY_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define RGD_IDENTIFY_HIDDEN __attribute__((visibility("hidden")))
#else
#define RGD_IDENTIFY_HIDDEN
#endif

#define IDENT_PARAM_MESSAGES_SENT      0x0006
#define IDENT_PARAM_MESSAGES_RECEIVED  0x0007

/* Exactly the set altscreen_v33_rgi_config.sh writes to dio_manager.json. */
#define RGD_IDENT_SENT_COUNT      2u
#define RGD_IDENT_RECEIVED_COUNT  3u
RGD_IDENTIFY_HIDDEN extern const uint16_t rgd_ident_sent_ids[RGD_IDENT_SENT_COUNT];
RGD_IDENTIFY_HIDDEN extern const uint16_t rgd_ident_received_ids[RGD_IDENT_RECEIVED_COUNT];

/* Locate parameter param_id in an Identify frame of len bytes.  The frame
 * must be well formed: 0x4040 sync, header length == len, and parameters that
 * tile [6, len) exactly.  Returns 1 and sets *off and *plen (parameter start and
 * total length including its 4-byte header) when found, 0 when the frame is
 * well formed but has no such parameter, -1 when the frame is malformed. */
RGD_IDENTIFY_HIDDEN int rgd_ident_find_param(const uint8_t *frame, size_t len, uint16_t param_id,
                                             size_t *off, size_t *plen);

/* Number of ids missing from list parameter param_id; -1 when the frame is
 * malformed, the parameter is absent, or its data is not a uint16 list. */
RGD_IDENTIFY_HIDDEN int rgd_ident_missing_ids(const uint8_t *frame, size_t len, uint16_t param_id,
                                              const uint16_t *ids, size_t n);

/* Append the missing ids to the end of list parameter param_id, growing the
 * parameter and the frame header length.  cap is the buffer capacity.
 * Returns the new frame length (len when nothing was missing), or 0 with the
 * frame untouched when it is malformed, the list is absent, or cap is short. */
RGD_IDENTIFY_HIDDEN size_t rgd_ident_add_ids(uint8_t *frame, size_t len, size_t cap, uint16_t param_id,
                                             const uint16_t *ids, size_t n);

/* Format list parameter param_id as "5000,5001,..." (hex, no 0x) into out.
 * Returns the number of ids, or -1 under the rgd_ident_missing_ids rules. */
RGD_IDENTIFY_HIDDEN int rgd_ident_format_ids(const uint8_t *frame, size_t len, uint16_t param_id,
                                             char *out, size_t out_size);

/* Format the parameter IDs of any iAP2 frame payload as "6,7,1e" (hex).
 * Used for IdentificationRejected (0x1D03), whose parameters are the IDs the
 * phone refused.  Returns the count, -1 when the frame is malformed. */
RGD_IDENTIFY_HIDDEN int rgd_ident_format_param_ids(const uint8_t *frame, size_t len,
                                                   char *out, size_t out_size);

#endif /* RGD_IDENTIFY_H */
