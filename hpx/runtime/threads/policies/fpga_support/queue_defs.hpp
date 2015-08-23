// This file was automatically created from queue_defs.v. Do not edit.
//// Multi-Queue definitions

// address bits assignment:
// higher-order bits of the address determine the queue number;
// MQ_REQ_BITS is the number of LSBits in the address containing the
// request code
#define MQ_REQ_BITS 5
// queue access requests
#define MQ_REQ_NOOP      0x0
// the following are or-able, but only one of GET/SET may be present at a time
#define MQ_REQ_SET_HEAD  0x1
#define MQ_REQ_SET_LAST  0x2
#define MQ_REQ_GET_HEAD  0x4
#define MQ_REQ_GET_LAST  0x8
// queue management and configuration requests
#define MQ_REQ_MGMT_BIT 4
// wrapper level
#define MQ_REQ_GET_WSIZE 0x1f
#define MQ_REQ_GET_NQ    0x1e
// queue level
#define MQ_REQ_RESET     0x1d
#define MQ_REQ_GET_SIZE  0x1c
#define MQ_REQ_GET_CNT   0x1b
#define MQ_REQ_GET_STAT  0x1a

//// status codes:
#define MQ_STAT_BITS 4
#define MQ_STAT_SUCCESS 0x0
#define MQ_STAT_INVALID 0x1
#define MQ_STAT_NOSPACE 0x2
#define MQ_STAT_EMPTY   0x3

