/*
 * ndav.h : a small program for WebDAV operations.
 *
 * Copyright (c) 2002 Yuuichi Teranishi <teranisi@gohome.org>
 * For license terms, see the file COPYING in this directory.
 *
 * Copyright (c) 2009, 2012 Mats Erik Andersson <meand@users.berlios.de>
 *
 * $Id$
 *
 * vim: set ai cin sw=4 ts=4:
 */

#ifndef _ND_H
#define _ND_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <libxml/tree.h>

/* Formatting and content extents.
 * Bit-OR determine outcome. */
#define ND_PRINT_AS_HEADER	0
#define ND_PRINT_AS_SEXP	1
#define ND_PRINT_QUIETLY	4
#define ND_PRINT_NAMEONLY	8
#define NDAV_PRINT_HEADER(x)	( ! ((x) & ND_PRINT_AS_SEXP) )
#define NDAV_PRINT_SEXP(x)		((x) & ND_PRINT_AS_SEXP)
#define NDAV_PRINT_QUIETLY(x)	((x) & ND_PRINT_QUIETLY)
#define NDAV_PRINT_VERBOSELY(x)	( ! ((x) & ND_PRINT_QUIETLY) )
#define NDAV_PRINT_NAMEONLY(x)	((x) & ND_PRINT_NAMEONLY)

#define ND_LOCK_SCOPE_EXCLUSIVE	101
#define ND_LOCK_SCOPE_SHARED	102
#define ND_LOCK_REQUEST_MAX		1024

/* DEPTH */
#define ND_DEPTH_0	0
#define ND_DEPTH_1	1
#define ND_DEPTH_INFINITE	2

/* HTTP/WebDAV Reason-Phrase of HTTP status line. */
#define ND_CODE_OTHER "System Error"
#define ND_CODE_100 "Continue"
#define ND_CODE_101 "Switching Protocols"
#define ND_CODE_102 "Processing" /* DAV */
#define ND_CODE_200 "OK"
#define ND_CODE_201 "Created"
#define ND_CODE_202 "Accepted"
#define ND_CODE_203 "Non-Authoritative Information"
#define ND_CODE_204 "No Content"
#define ND_CODE_205 "Reset Content"
#define ND_CODE_206 "Partial Content"
#define ND_CODE_207 "Multi-Status" /* DAV */
#define ND_CODE_300 "Multiple Choices"
#define ND_CODE_301 "Moved Permanently"
#define ND_CODE_302 "Found"
#define ND_CODE_303 "See Other"
#define ND_CODE_304 "Not Modified"
#define ND_CODE_305 "Use Proxy"
#define ND_CODE_307 "Temporary Redirect"
#define ND_CODE_400 "Bad Request"
#define ND_CODE_401 "Unauthorized"
#define ND_CODE_402 "Payment Required"
#define ND_CODE_403 "Forbidden"
#define ND_CODE_404 "Not Found"
#define ND_CODE_405 "Method Not Allowed"
#define ND_CODE_406 "Not Acceptable"
#define ND_CODE_407 "Proxy Authentication Required"
#define ND_CODE_408 "Request Time-out"
#define ND_CODE_409 "Conflict"
#define ND_CODE_410 "Gone"
#define ND_CODE_411 "Length Required"
#define ND_CODE_412 "Precondition Failed"
#define ND_CODE_413 "Request Entity Too Large"
#define ND_CODE_414 "Request-URI Too Large"
#define ND_CODE_415 "Unsupported Media Type"
#define ND_CODE_416 "Requested range not satisfiable"
#define ND_CODE_417 "Expectation Failed"
#define ND_CODE_422 "Unprocessable Entity" /* DAV */
#define ND_CODE_423 "Locked" /* DAV */
#define ND_CODE_424 "Failed Dependency" /* DAV */
#define ND_CODE_500 "Internal Server Error"
#define ND_CODE_501 "Not Implemented"
#define ND_CODE_502 "Bad Gateway"
#define ND_CODE_503 "Service Unavailable"
#define ND_CODE_504 "Gateway Time-out"
#define ND_CODE_505 "HTTP Version not supported"
#define ND_CODE_507 "Insufficient Storage" /* DAV */

#define ND_HEADER_LINE_MAX	1024
#define ND_REQUEST_MAX		2048

/*
 * ndLockInfo :
 * Lock structure for LOCK, UNLOCK.
 */
typedef struct _nd_lock_info
{
	struct _nd_lock_info * next;
	char * scope;
	char * type;
	char * owner_href;
	char * token;
	char * depth;
	char * timeout;
} ndLockInfo, * ndLockInfoPtr;

/*
 * ndProp :
 * Property structure
 */
typedef struct _nd_prop
{
	struct _nd_prop * next;
	char * ns;
	char * name;
	char * value;
} ndProp, * ndPropPtr;

/*
 * ndNodeInfo :
 * Node info structure
 */
typedef struct _nd_node_info
{
	struct _nd_node_info * next;
	char * name;
	char * size;
	char * date;
	char * cdate;
	char * content;
	char * restype;
	int status;
	ndPropPtr props;
	ndLockInfoPtr lock;
} ndNodeInfo, * ndNodeInfoPtr;

extern ndNodeInfoPtr ndNodeInfoNew(void);
extern void ndNodeInfoFree(ndNodeInfoPtr info);
extern void ndNodeInfoListFree(ndNodeInfoPtr info);
extern void ndNodeInfoPrint(FILE * fp, ndNodeInfoPtr info, int format);
extern void ndNodeInfoListPrint(FILE * fp, ndNodeInfoPtr info, int format);

extern char * ndReasonPhrase(int code);

extern ndLockInfoPtr ndLockInfoNew(void);
extern void ndLockInfoFree(ndLockInfoPtr lock);
extern void ndLockInfoListFree(ndLockInfoPtr lock);
extern void ndLockInfoPrint(FILE * fp, ndLockInfoPtr lock, int format);
extern void ndLockInfoListPrint(FILE * fp, ndLockInfoPtr lock, int format);

extern ndPropPtr ndPropNew(void);
extern void ndPropFree(ndPropPtr prop);
extern void ndPropListFree(ndPropPtr prop);
extern void ndPropPrint(FILE * fp, ndPropPtr prop, int format);
extern void ndPropListPrint(FILE * fp, ndPropPtr prop, int format);

typedef struct _nd_auth_param
{
	char * name;
	char * val;
} ndAuthParam, * ndAuthParamPtr;

/* Authentication API */

/*
 * A callback to obtain an authenticate result.
 */
typedef void (*ndAuthResultCallback) (int resultCode);

/*
 * A callback to set up Authentication Parameters.
 */
typedef int (*ndAuthCallback)(ndAuthParamPtr param, int is_proxy);

/*
 * A callback to notify Authentication end.
 */
typedef void (*ndAuthNotifyCallback) (void * ctxt);

typedef struct _nd_auth_ctxt
{
	ndAuthCallback auth_cb;
	ndAuthNotifyCallback notify_cb;
	char * auth_realm;	/* Authentication realm. */
	char * auth_type;	/* Type of authentication.  */
	char * pauth_realm; /* Proxy authentication realm.  */
	char * pauth_type;	/* Type of proxy authentication.  */
} ndAuthCtxt, * ndAuthCtxtPtr;

extern ndAuthCtxtPtr ndCreateAuthCtxt(ndAuthCallback auth_cb, 
						ndAuthNotifyCallback notify_cb,
						char * auth_realm,
						char * pauth_realm);
extern void ndFreeAuthCtxt(ndAuthCtxtPtr auth);

/* Auth param accessors. */
extern char * ndAuthParamValue(ndAuthParamPtr param, char * name);
extern int	ndAuthParamSetValue(ndAuthParamPtr param,
								char * name, char * val);

/*
 * Allocated Header String Buffer is set to buf_return,
 */
extern int ndAuthCreateHeader(char * str,
					ndAuthCallback fn,
					xmlBufferPtr * buf_return,
					int is_proxy);

/* DAV API */
extern int ndPropFind(char * url,
				ndAuthCtxtPtr auth,
				char * prop,
				char * ns,
				int depth,
				int detect,
				ndNodeInfoPtr * ni_return);

extern int ndPropPatch(char * url,
				ndAuthCtxtPtr auth,
				char * prop,
				char * value,
				char * ns,
				char * lock_token,
				ndNodeInfoPtr * ni_return);

extern int ndLock(char * url,
			ndAuthCtxtPtr auth,
			int depth,
			char * owner,
			int scope,
			char * timeout,
			ndLockInfoPtr * li_return);

extern int ndUnlock(char * url,
			ndAuthCtxtPtr auth,
			int depth,
			char * lock_token);

extern int ndMkCol(char * url,
				ndAuthCtxtPtr auth,
				char * lock_token);

extern int ndPut(char * url,
			ndAuthCtxtPtr auth,
			char * content,
			int length,
			char * lock_token,
			ndNodeInfoPtr * ni_return);

extern int ndPost(char * url,
			ndAuthCtxtPtr auth,
			char * content,
			int length,
			char **content_type,
			xmlBufferPtr * buf_return);

extern int ndPostPrint(char * url,
			ndAuthCtxtPtr auth,
			char * content,
			int length,
			char **content_type,
			FILE * outfp);

extern int ndGet(char * url,
			ndAuthCtxtPtr auth,
			char **ct_return,
			xmlBufferPtr * buf_return);

extern int ndGetPrint(char * url,
				ndAuthCtxtPtr auth,
				char **ct_return,
				FILE * outfp);

extern int ndDelete(char * url,
			ndAuthCtxtPtr auth,
			char * lock_token);

extern int ndMove(char * url,
			ndAuthCtxtPtr auth,
			char * dest_url,
			int overwrite,
			char * lock_token);

extern int ndCopy(char * url,
			ndAuthCtxtPtr auth,
			char * dest_url,
			int overwrite,
			char * lock_token);

#endif /* _ND_H */
