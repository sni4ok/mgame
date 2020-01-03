/*
        Copyright (c) 2012, MICEX-RTS. All rights reserved.

        Plaza-2 Client Gate API Description. Included are types,
        constants and functions related to the API.
        Contents are subject to change.

        All the software and documentation included in this and any
        other MICEX-RTS CGate Releasese is copyrighted by MICEX-RTS.

        Redistribution and use in source and binary forms, with or without
        modification, are permitted only by the terms of a valid
        software license agreement with MICEX-RTS.

        THIS SOFTWARE IS PROVIDED "AS IS" AND MICEX-RTS DISCLAIMS ALL WARRANTIES
        EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
        NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
        PURPOSE.  MICEX-RTS DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
        UNINTERRUPTED OR ERROR-FREE.  MICEX-RTS SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
        LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
        INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
        TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF MICEX-RTS HAS BEEN APPRISED OF
        THE LIKELIHOOD OF SUCH DAMAGES.
*/

#ifndef CGATE_HEADER
#define CGATE_HEADER

// ------------------------------------------------------------------
// platform detection
#if defined(_WIN32)
#define CG_PLATFORM_WINDOWS 1
#elif defined(__linux__)
#define CG_PLATFORM_LINUX 1
#elif defined(__APPLE__)
#define CG_PLATFORM_MACOS 1
#else //platform detection
#error "Client Gate: Platform is not supported."
#endif //platform detection
// ------------------------------------------------------------------

// ------------------------------------------------------------------
// compiler detection
#if defined(_MSC_VER)

#define CG_COMPILER_MSVC 1

#if defined(_M_X64) || defined(_M_AMD64)
#define CG_ARCH_64 1
#elif defined(_M_IX86) || defined(_X86_)
#define CG_ARCH_32 1
#else
#error "Client Gate: Unsupported architecture"
#endif //architecture
#elif defined(__GNUC__)

#define CG_COMPILER_GCC 1

#if defined(__x86_64) || defined(__x86_64__)
#define CG_ARCH_64 1
#elif defined(_X86_) || defined(__i386__) || defined(i386)
#define CG_ARCH_32 1
#else
#error "Client Gate: Unsupported architecture"
#endif // architecture

#if defined(__MINGW32__)

#define CG_COMPILER_MINGW 1

#if defined(__x86_64) || defined(__x86_64__)
#define CG_ARCH_64 1
#elif defined(_X86_) || defined(__i386__) || defined(i386)
#define CG_ARCH_32 1
#else
#error "Client Gate: Unsupported architecture"
#endif // architecture

#endif // defined(__MINGW32__)
#else
#error "RTS Client Gate: Compiler is not supported. "
#endif // compiler detection
// ------------------------------------------------------------------

// ------------------------------------------------------------------
// system includes
#ifdef CG_COMPILER_GCC
#include <stdint.h>
#include <stddef.h>
#else

#ifndef int8_t
typedef signed __int8     int8_t;
#endif //int8_t
#ifndef int16_t
typedef signed __int16    int16_t; 
#endif //int16_t
#ifndef int32_t
typedef signed __int32    int32_t;
#endif //int32_t
#ifndef int64_t
typedef signed __int64    int64_t;
#endif //int64_t
#ifndef uint8_t
typedef unsigned __int8   uint8_t;
#endif //uint8_t
#ifndef uint16_t
typedef unsigned __int16  uint16_t;
#endif //uint16_t
#ifndef uint32_t
typedef unsigned __int32  uint32_t;
#endif //uint32_t
#ifndef uint64_t
typedef unsigned __int64  uint64_t;
#endif //uint64_t

#endif
#include <stdarg.h>
// ------------------------------------------------------------------

// ------------------------------------------------------------------
// API setup
#define CG_VERSION_MAJOR 5
#define CG_VERSION_MINOR 5
#define CG_VERSION_PATCH 0
#define CG_MAKE_VERSION(a,b,c) (((a)<<24) + ((b)<<16) + (c))
#define CG_VERSION CG_MAKE_VERSION(CG_VERSION_MAJOR, CG_VERSION_MINOR, CG_VERSION_PATCH)

// ------------------------------------------------------------------
// enumerations



// error codes
#define CG_RANGE_BEGIN 0x20000

enum {
	CG_ERR_OK = 0,
	CG_ERR_INTERNAL = CG_RANGE_BEGIN,
	CG_ERR_INVALIDARGUMENT,
	CG_ERR_UNSUPPORTED,
	CG_ERR_TIMEOUT,
	CG_ERR_MORE,
	CG_ERR_INCORRECTSTATE,
	CG_ERR_DUPLICATEID,
	CG_ERR_BUFFERTOOSMALL,
	CG_ERR_OVERFLOW,
	CG_ERR_UNDERFLOW,
	CG_ERR_EOF,
	CG_RANGE_END
};

// scheme types
#define CG_SCHEME_BINARY 1

#define CG_SCHEME_BIN_MSG_HAS_ID					0x00000001
#define CG_SCHEME_BIN_MSG_HAS_NAME					0x00000002
#define CG_SCHEME_BIN_MSG_HAS_DESC					0x00000004
#define CG_SCHEME_BIN_MSG_HAS_INDICES				0x00000008
#define CG_SCHEME_BIN_MSG_HAS_TIMESTAMP_HINT		0x00000010
#define CG_SCHEME_BIN_MSG_HAS_USER_HINT				0x00000020
#define CG_SCHEME_BIN_FIELD_HAS_ID					0x00000100
#define CG_SCHEME_BIN_FIELD_HAS_NAME				0x00000200
#define CG_SCHEME_BIN_FIELD_HAS_DESC				0x00000400
#define CG_SCHEME_BIN_FIELD_HAS_TYPE				0x00000800
#define CG_SCHEME_BIN_FIELD_HAS_DEFVAL				0x00001000

// states
#define CG_STATE_CLOSED		0
#define CG_STATE_ERROR		1
#define CG_STATE_OPENING	2
#define CG_STATE_ACTIVE		3

// message creation flags
#define CG_KEY_INDEX		0
#define CG_KEY_ID			1
#define CG_KEY_NAME			2

// listener flags

// publisher flags
#define CG_PUB_NEEDREPLY		0x1

// message types
#define CG_MSG_OPEN			    0x100
#define CG_MSG_CLOSE			0x101
#define CG_MSG_TIME			    0x102
#define CG_MSG_DATA			    0x110
#define CG_MSG_STREAM_DATA		0x120
#define CG_MSG_DATAARRAY		0x150
#define CG_MSG_OBJFAILED		0x180
#define CG_MSG_PARSEERR			0x190


#define CG_MSG_TN_BEGIN			0x200
#define CG_MSG_TN_COMMIT		0x210
#define CG_MSG_TN_ROLLBACK		0x220

#define CG_MSG_P2MQ_RANGE_START		0x1000
#define CG_MSG_P2MQ_TIMEOUT			(0x1 + CG_MSG_P2MQ_RANGE_START)

#define CG_MSG_P2REPL_RANGE_START		0x1100
#define CG_MSG_P2REPL_LIFENUM			(0x10 + CG_MSG_P2REPL_RANGE_START)
#define CG_MSG_P2REPL_CLEARDELETED		(0x11 + CG_MSG_P2REPL_RANGE_START)
#define CG_MSG_P2REPL_ONLINE			(0x12 + CG_MSG_P2REPL_RANGE_START)
#define CG_MSG_P2REPL_REPLSTATE			(0x15 + CG_MSG_P2REPL_RANGE_START)

#define CG_REASON_UNDEFINED 0
#define CG_REASON_USER 1
#define CG_REASON_ERROR 2
#define CG_REASON_DONE 3
#define CG_REASON_SNAPSHOT_DONE 4

#define CG_REPLACT_INSERT 0
#define CG_REPLACT_UPDATE 1
#define CG_REPLACT_DELETE 2
#define CG_REPLACT_INSUPD 3

// cg_data_lifenum_t flags
#define CG_LIFENUM_CHANGE__DONT_CLEAR_STORAGE 0x1

#define CG_INDEX_INVALID ((size_t)-1)
#define CG_ID_INVALID ((uint32_t)-1)
#define CG_MAX_REVISON ((int64_t)0x7FFFFFFFFFFFFFFF)

#define CG_OWNER_UNKNOWN ((int64_t)0)



#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#ifdef CG_COMPILER_MSVC
#ifdef CG_EXPORTS
#define CG_EXPORT __declspec(dllexport)
#else
#define CG_EXPORT __declspec(dllimport)
#endif //CG_EXPORTS
#elif __GNUC__
#define CG_EXPORT __attribute__ ((visibility ("default")))
#else
#define CG_EXPORT
#endif //CG_COMPILER_MSVC

#ifdef _WIN32
#define CG_API __cdecl
#elif __i386__
#define CG_API __attribute__((cdecl))
#else
#define CG_API
#endif//_WIN32

typedef uint32_t CG_RESULT;

typedef struct cg_conn_t cg_conn_t;
typedef struct cg_listener_t cg_listener_t;
typedef struct cg_publisher_t cg_publisher_t;
typedef struct cg_scheme_t cg_scheme_t;
typedef struct cg_logger_t cg_logger_t;

/**
 General purpose message
 */
struct cg_msg_t
{
	uint32_t type;		/// Message type
	size_t data_size;	/// Data size
	void* data;			/// Data pointer
	int64_t owner_id;	/// Message owner id
};

/**
 * Time message
 */
struct cg_msg_time_t
{
	uint32_t type;		// Message type
	size_t data_size;	// Data size
	void* data;			// Data pointer
	int64_t owner_id;	/// Message owner id
	uint64_t usec;		// Microseconds from epoch
};

/**
 Stream data message
 */
struct cg_msg_streamdata_t 
{
	uint32_t type;			/// Message type = CG_MSG_STREAM_DATA
	size_t data_size;		/// Data size
	void* data;				/// Data pointer
	int64_t owner_id;	/// Message owner id

	size_t msg_index;		/// Message number in active scheme
	uint32_t msg_id;		/// Unique message ID (if applicable)
	const char* msg_name;	/// Message name in active scheme

	int64_t rev;			/// Message sequence number
	size_t num_nulls;		/// Size of presence map
	const uint8_t* nulls;   /// Presence map. Contains 1 for NULL fields
	uint64_t user_id;       /// User ID message is intended for
};

/**
 Data message
 */
struct cg_msg_data_t
{
	uint32_t type;			/// Message type = CG_MSG_P2REPL_DATA 
	size_t data_size;		/// Data size
	void* data;				/// Data pointer
	int64_t owner_id;	/// Message owner id

	size_t msg_index;		/// Message number in active scheme
	uint32_t msg_id;		/// Unique message ID (if applicable)
	const char* msg_name;	/// Message name in active scheme

	union {
		uint32_t user_id;
		void * user_ptr;
	};				/// User message id (the value sent back in reply)

	const char* addr;		/// Remote address
	struct cg_msg_data_t* ref_msg;	/// Reference message (not used now)
};

/**
 Data message
 */
struct cg_msg_dataarray_t
{
	uint32_t type;			/// Message type = CG_MSG_P2REPL_DATA 
	size_t data_size;		/// Data size
	void* data;				/// Data pointer
	int64_t owner_id;	/// Message owner id

	size_t msg_index;		/// Message number in active scheme
	uint32_t msg_id;		/// Unique message ID (if applicable)
	const char* msg_name;	/// Message name in active scheme

	union {
		uint32_t user_id;
		void * user_ptr;
	};				/// User message id (the value sent back in reply)

	const char* addr;		/// Remote address
	struct cg_msg_data_t* ref_msg;	/// Reference message (not used now)
	uint32_t cnt;
};

/// User message callback function
typedef CG_RESULT (CG_API *CG_LISTENER_CB)(cg_conn_t* conn, cg_listener_t* listener, struct cg_msg_t* msg, void* data);

/// List of key-value pairs
struct cg_value_pair_t {
	/// Pointer to the next list entry
	struct cg_value_pair_t *next;
	/// Key, required
	char * key;
	/// Value, may be null
	char * value;
};

/// Field value description
struct cg_field_value_desc_t {
	// pointer to the next value
	struct cg_field_value_desc_t* next;

	// name 
	char* name;

	// description
	// May be NULL if not defined
	char* desc; 

	// pointer to value
	void* value; 

	// used for integer fields only (i[1-8], u[1-8])
	// a mask that defines range of bits used by the value
	void* mask;
};

struct cg_message_desc_t;

/// Message field description
struct cg_field_desc_t {
	/// pointer to the next value
	struct cg_field_desc_t* next;

	/// Field ID. May be 0 if not defined
	uint32_t id;

	/// Field name
	char* name;

	/// Field description
	/// May be NULL if not defined
	char* desc;

	/// field type
	char* type;

	/// size in bytes
	size_t size;

	/// offset from buffer beginning
	size_t offset;

	/// Pointer to default value of the field
	/// Points to the buffer of size "size"
	/// May be NULL if no default value is defined
	void* def_value;

	/// Number of values for the field values
	size_t num_values;

	/// Pointer to the list of values
	/// which can be taken by the field
	struct cg_field_value_desc_t* values;

	/// Field options
	struct cg_value_pair_t* hints;

	/// Maximum number of fields, 1 by default
	size_t max_count;

	/// Link to description of count field
	struct cg_field_desc_t * count_field;

	/// Pointer to message description for type = 'm' fields
	struct cg_message_desc_t * type_msg;
};

// Index component description
struct cg_indexfield_desc_t {
	// Pointer to the next value
	struct cg_indexfield_desc_t* next;

	// points to field of index 
	struct cg_field_desc_t* field;

	// sort order (0 for asc, 1 for desc)
	uint32_t sort_order;
};

// Index description
struct cg_index_desc_t {
	// Pointer to the next index
	struct cg_index_desc_t * next;

	// Number of fields in the index
	size_t num_fields;

	// Pointer to the first index component
	struct cg_indexfield_desc_t* fields;

	// name
	char* name;

	// desc
	// may be NULL if not defined
	char* desc;

	// index hints
	struct cg_value_pair_t* hints;
};

// Message description
struct cg_message_desc_t {
	// Pointer to the next message
	struct cg_message_desc_t* next;

	// Message data block size
	size_t size;

	// Number of fields in the message
	size_t num_fields;

	// Pointer to the first message description
	struct cg_field_desc_t* fields;

	// Message unique ID
	// May be 0 if not defined
	uint32_t id;

	// Message name
	// May be NULL if not defined
	char *name;

	// Description
	// May be NULL if not defined
	char *desc;

	// Message hints
	// May be NULL if not defined
	struct cg_value_pair_t* hints;

	// Number of indices for the message
	size_t num_indices;

	// Pointer to the first index description
	struct cg_index_desc_t* indices;

	// Size of alignment
	size_t align;
};

// Scheme description
struct cg_scheme_desc_t {
	// Scheme type
	// 1 for binary scheme which is the only type supported now
	uint32_t scheme_type;

	// Scheme features (combination of CG_SCHEME_BIN_* constants)
	uint32_t features;

	// Number of messages in the scheme
	size_t num_messages;

	// Pointer to the first message description
	struct cg_message_desc_t* messages;

	// Scheme options
	struct cg_value_pair_t* hints;
};

#pragma pack(push,4)

/*!
 Date-time type
 */
struct cg_time_t {
	uint16_t year; /// Year
	uint8_t month; /// Month of year (1-12)
	uint8_t day; /// Day of month (1-31)
	uint8_t hour; /// Hour (0-23)
	uint8_t minute; /// Minute (0-59)
	uint8_t second; /// Second (0-59)
	uint16_t msec; /// Millisecond (0-999)
};

/*!
 Structure that describes data format
 for CG_MSG_P2REPL_CLEARDELETED message
 */
struct cg_data_cleardeleted_t {
	uint32_t table_idx;
	int64_t table_rev;
};

struct cg_repl_act_t {
	uint8_t act; // INSERT, UPDATE, DELETE
	uint8_t idx_id;
	char reserved[6];
};

struct cg_data_lifenum_t {
	uint32_t life_number;
	uint32_t flags; // CG_LIFENUM_CHANGE_
};

struct cg_zblock_t {
	uint32_t size;
	uint64_t data_ptr;
};

#pragma pack(pop)

// -----------------------------------------------------------------------------
// environment
// -----------------------------------------------------------------------------

/*!
 Initialize environment
 */
CG_EXPORT CG_RESULT CG_API cg_env_open(const char* settings);
/*!
 Deinitialize environment
 */
CG_EXPORT CG_RESULT CG_API cg_env_close();

/**
 Create new connection
 @param settings Connection initialization string
 @param connptr Where pointer to new connection will be stored

 Connection initialization string has the following format:
 "TYPE://HOST:PORT&param1=value1&param2=value2..."
 ,where
 - TYPE defines connection type
   - "p2tcp" means TCP connection with Plaza-2 router process
   - "p2lrcpq" meanes shared memory connection with Plaza-2 router process
 - HOST is host of the machine where Plaza-2 router runs
 - PORT is port of Plaza-2 router
 - param1 ... paramN - parameters which depend on type of connection.
   
 Parameters for "p2tcp" and "p2lrpcq" connections:
 - app_name - a string, Plaza-2 application name for connection
            each connection must have unique name
 - timeout - a number, timeout of Plaza-2 router connection, ms
 - local_timeout - a number, timeout of Plaza-2 router interactions, ms 
 */
CG_EXPORT CG_RESULT CG_API cg_conn_new(const char* settings, cg_conn_t** connptr);

/** Destroys connection */
CG_EXPORT CG_RESULT CG_API cg_conn_destroy(cg_conn_t* conn);

/**
  Open connection
  @param settings Connection open string
  Parameter "settigns" is not used for both p2tcp and p2lrpcq connection types.
*/
CG_EXPORT CG_RESULT CG_API cg_conn_open(cg_conn_t* conn, const char* settings);

/** Close connection */
CG_EXPORT CG_RESULT CG_API cg_conn_close(cg_conn_t* conn);

/** Get connection state */
CG_EXPORT CG_RESULT CG_API cg_conn_getstate(cg_conn_t* conn, uint32_t* state);

/** Process one iteration of connection internal logic */
CG_EXPORT CG_RESULT CG_API cg_conn_process(cg_conn_t* conn, uint32_t timeout, void* reserved);


/** Create new listener
   @param conn Connection to create listener on
   @param settings Connection initialization string
   @param cb Pointer to user message callback
   @param data User data that will be passed as one of the callback parameters
   @param lsnptr Where pointer to new listener will be stored

   There are following types of listeners supported:
   p2repl - datastream replication client
   p2mqreply - messages reply listener
   p2ordbook - optimized orderbook datastream listener

   Initialization string for "p2repl":
   p2repl://STREAM[;scheme=SCHEMEURL]
     * STREAM - datastream name
	 * SCHEMEURL - scheme URL in format "|SRC|PATH|NAME"
	   where SRC is either FILE or MQ,
	         PATH is path to scheme INI file for FILE source
			 PATH is scheme service name for MQ soruce
			 NAME is name of the scheme in file or scheme service

   Initialization string for "p2mqreply":
   p2mqreply://;ref=REFERENCE
     * REFERENCE - name of the publisher that is used to send messages

   Initialization string for "p2ordbook":
   p2ordbook://STREAM;snapshot=STREAMSS[;scheme=SCHEME][;snapshot.scheme=SCHEMESS]
     * STREAM - name of orderbook online stream
	 * STREAMSS - name of orderbook snapshot stream
	 * SCHEME - scheme URL for online stream
	 * SCHEMESS - scheme URL for snapshot stream	 

 */
CG_EXPORT CG_RESULT CG_API cg_lsn_new(cg_conn_t* conn, const char* settings, CG_LISTENER_CB cb, void* data, cg_listener_t** lsnptr);
 
/** Destroy listener */
CG_EXPORT CG_RESULT CG_API cg_lsn_destroy(cg_listener_t* lsn);

/** Get listener state */
CG_EXPORT CG_RESULT CG_API cg_lsn_getstate(cg_listener_t* lsn, uint32_t* state);

/** Open listener
	@param settings Listener open string

	Open string for "p2repl":
	[mode=STREAMTYPES][;replstate=REPLSTATE]
		* STREAMTYPES - stream mode ('snapshot', 'online', 'snapshot+online')
		* REPLSTATE - previously saved stream stated (string received in CG_MSG_P2REPL_REPLSTATE message)
		
	Parameter "settings" is ignored for other types of listeners.

*/
CG_EXPORT CG_RESULT CG_API cg_lsn_open(cg_listener_t* lsn, const char* settings);

/** Close listener */
CG_EXPORT CG_RESULT CG_API cg_lsn_close(cg_listener_t* lsn);

/** Get current data scheme for listener */
CG_EXPORT CG_RESULT CG_API cg_lsn_getscheme(cg_listener_t* lsn, struct cg_scheme_desc_t** desc);

/** Create publisher

	There are following types of publishers supported::
	p2mq - posts messages to specified Plaza-2 service
	
	Initialization string for "p2mq":
	p2mq://SERVICE[;scheme=SCHEMEURL][;timeout=TIMEOUT][;name=NAME]
		* SERVICE - destination service name
		* SCHEMEURL - scheme URL
		* TIMEOUT - reply timeout, ms
		* NAME - unique publisher name to be referenced by p2mqreply listener

 */
CG_EXPORT CG_RESULT CG_API cg_pub_new(cg_conn_t* conn, const char* settings, cg_publisher_t** pubptr);
 
/** Destroy publisher */
CG_EXPORT CG_RESULT CG_API cg_pub_destroy(cg_publisher_t* pub);
 
/** Open publisher 
  @param settings Publisher open string

  Parameter "settings" is not used currently and should be empty.
*/
CG_EXPORT CG_RESULT CG_API cg_pub_open(cg_publisher_t* pub, const char* settings);
 
/** Close publisher */
CG_EXPORT CG_RESULT CG_API cg_pub_close(cg_publisher_t* pub);
 
/** Get publisher state */
CG_EXPORT CG_RESULT CG_API cg_pub_getstate(cg_publisher_t* pub, uint32_t* state);

/** Get data scheme description */
CG_EXPORT CG_RESULT CG_API cg_pub_getscheme(cg_publisher_t* pub, struct cg_scheme_desc_t** desc);


/** Create new message for publishing
   @param id_type CG_KEY_INDEX Create message using its index in current publisher scheme (id points to uint32_t)
   @param id_type CG_KEY_ID Create message using its unique ID in publisher scheme (id points to uint32_t)
   @param id_type CG_KEY_NAME Create message using its name in current publisher scheme (id points to 0-terminated string)
*/
CG_EXPORT CG_RESULT CG_API cg_pub_msgnew(cg_publisher_t* pub, uint32_t id_type, const void* id, struct cg_msg_t** msgptr);

/** Free message */
CG_EXPORT CG_RESULT CG_API cg_pub_msgfree(cg_publisher_t* pub, struct cg_msg_t* msgptr);

/** Post message
   @param flags Flags of post operation
          CG_PUB_NEEDREPLY - if message reply is expected   
*/
CG_EXPORT CG_RESULT CG_API cg_pub_post(cg_publisher_t* pub, struct cg_msg_t* msg, uint32_t flags);

/**
 Return string representation for error code.
 @param errCode CGate error code
*/
CG_EXPORT const char* CG_API cg_err_getstr(CG_RESULT errCode);

/**
 Log user message
 @param fmt Format string (printf-like)
 @param ... Parameters
 */
CG_EXPORT CG_RESULT CG_API cg_log_trace(const char* fmt, ...);
CG_EXPORT CG_RESULT CG_API cg_log_debug(const char* fmt, ...);
CG_EXPORT CG_RESULT CG_API cg_log_info(const char* fmt, ...);
CG_EXPORT CG_RESULT CG_API cg_log_error(const char* fmt, ...);
CG_EXPORT CG_RESULT CG_API cg_log_warning(const char* fmt, ...);

/**
 Log user message
 @param fmt Format string (printf-like)
 @param ... Parameters
 */
CG_EXPORT CG_RESULT CG_API cg_log_va_trace(const char* fmt, va_list va);
CG_EXPORT CG_RESULT CG_API cg_log_va_debug(const char* fmt, va_list va);
CG_EXPORT CG_RESULT CG_API cg_log_va_info(const char* fmt, va_list va);
CG_EXPORT CG_RESULT CG_API cg_log_va_error(const char* fmt, va_list va);
CG_EXPORT CG_RESULT CG_API cg_log_va_warning(const char* fmt, va_list va);

/**
 Log user message
 For languages that do not support variable number of arguments
 @param str String to log
 */
CG_EXPORT CG_RESULT CG_API cg_log_tracestr(const char* str);
CG_EXPORT CG_RESULT CG_API cg_log_debugstr(const char* str);
CG_EXPORT CG_RESULT CG_API cg_log_infostr(const char* str);
CG_EXPORT CG_RESULT CG_API cg_log_errorstr(const char* str);
CG_EXPORT CG_RESULT CG_API cg_log_warningstr(const char* str);

/**
 Dump specified message as a text. Message data will be parsed if data scheme is specified as paramater.
 This function is designed for debugging purposes.
 @param msg Message to dump
 @param scheme Data scheme (may be NULL)
 @param buffer Pointer to preallocated buffer where to store dump
 @param bufsize Points to variable holding buffer size. 

 If the buffer is too small for message dump, CG_ERR_BUFFERTOOSMALL error code will be returned and
 bufsize will be filled with desired buffer size.
 */
CG_EXPORT CG_RESULT CG_API cg_msg_dump(const struct cg_msg_t* msg, struct cg_scheme_desc_t* scheme, char* buffer, size_t* bufsize);

/**
 Return BCD value as integer part and position of decimal point.
 Returned values can be used the following way:
 val = bigdecimal(intpart) / 10^scale

 @param bcd Point to BCD value
 @param intpart Where to store integer part
 @param scale Where to store scale
 */
CG_EXPORT CG_RESULT CG_API cg_bcd_get(const void* bcd, int64_t *intpart, int8_t *scale);

/**
 Return data represented as text
 @param type Field type (the same format as used in schemes)
 @param data Data pointer
 @param buffer Pointer to buffer where string will be stored
 @param bufsize Points to variable holding buffer size

 If the buffer is too small for string representation, CG_ERR_BUFFERTOOSMALL error code will be
 returned and bufsize will be filled with desired buffer size.
 */
CG_EXPORT CG_RESULT CG_API cg_getstr(const char* type, const void* data, char* buffer, size_t* buffer_size);

/**
 Return 0 on success
 @param name name of the component
 @param major pointer to int where function put major version of the component
 @param minor pointer to int where function put minor version of the component
 @param patch pointer to int where function put patch version of the component
 */
CG_EXPORT CG_RESULT CG_API cg_env_getcomp_ver(const char* name, int* major, int* minor, int* patch);

/**
User exit event handler
@param data user context
 */
typedef void (CG_API *CG_EXITEVENT_CB)(void* data);

/**
Register User exit event handler
@param cb user exit event handler
@param cd_data user context
 */
CG_EXPORT CG_RESULT CG_API cg_env_register_exithandler(CG_EXITEVENT_CB cb, void* cb_data);

#ifdef CG_SHORT_NAMES

#define conn_t cg_conn_t
#define publisher_t cg_publisher_t
#define listener_t cg_listener_t

#define msg_t cg_msg_t
#define msg_streamdata_t cg_msg_streamdata_t
#define msg_data_t cg_msg_data_t

#define field_desc_t cg_field_desc_t
#define message_desc_t cg_message_desc_t
#define scheme_desc_t cg_scheme_desc_t

#define env_open cg_env_open
#define env_close cg_env_close

#define conn_new cg_conn_new
#define conn_destroy cg_conn_destroy
#define conn_open cg_conn_open
#define conn_close cg_conn_close
#define conn_getstate cg_conn_getstate
#define conn_process cg_conn_process

#define lsn_new cg_lsn_new
#define lsn_destroy cg_lsn_destroy
#define lsn_getstate cg_lsn_getstate
#define lsn_open cg_lsn_open
#define lsn_close cg_lsn_close
#define lsn_getscheme cg_lsn_getscheme

#define pub_new cg_pub_new
#define pub_destroy cg_pub_destroy
#define pub_getstate cg_pub_getstate
#define pub_open cg_pub_open
#define pub_close cg_pub_close
#define pub_msgnew cg_pub_msgnew
#define pub_msgfree cg_pub_msgfree
#define pub_post cg_pub_post
#define pub_getscheme cg_pub_getscheme

#define err_getstr cg_err_getstr

#define log_trace cg_log_trace
#define log_debug cg_log_debug
#define log_info cg_log_info
#define log_error cg_log_error

#endif //CG_SHORT_NAMES

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //CGATE_HEADER

