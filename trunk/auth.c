/*
 * auth.c - authentication module for ndav.
 *
 * Copyright (c) 2002 Yuuichi Teranishi <teranisi@gohome.org>
 * For license terms, see the file COPYING in this directory.
 *
 * Copyright (C) 2009 Mats Erik Andersson <meand@users.berlios.de>
 *
 * $Id$
 *
 * vim: set ai sw=4 ts=4:
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#  define _GNU_SOURCE
#  include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#ifdef HAVE_TIME
#  include <time.h>
#endif

#include "ndav.h"
#include <libxml/tree.h>

#if HAVE_MHASH_H
#  include <mhash.h>
#endif /* HAVE_MHASH_H */

#define SKIP_BLANKS(p) { while (*(p)&&isspace(*(p))) (p)++; }

/* Reused from mimehead.c */
static const char Base64Table[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

extern const char *export_method;
extern const char *export_uri;

#if HAVE_MHASH_H && USE_DIGEST
static int nc = 0;
static char nonce_sensor[64] = "";
#endif

struct http_auth {
	char * name;
	ndAuthParamPtr(* param_fn)(void);
	xmlBufferPtr(*auth_fn)(ndAuthParamPtr param);
};

ndAuthParamPtr ndAuthParamCreateBasic();
ndAuthParamPtr ndAuthParamCreateDigest();

xmlBufferPtr ndAuthBasic(ndAuthParamPtr param);
xmlBufferPtr ndAuthDigest(ndAuthParamPtr param);

xmlBufferPtr ndAuthEncodeDigest(xmlBufferPtr a);

struct http_auth www_auth[] =
{
	{"Basic",	ndAuthParamCreateBasic, ndAuthBasic},
	{"Digest",	ndAuthParamCreateDigest, ndAuthDigest},
	{NULL, NULL, NULL}
};

/* Short hand operator to add a single character. */
void xmlBufferAddChar(xmlBufferPtr buf, char ch) {
	xmlChar str[2];
	str[0] = ch;
	str[1] = '\0';
	xmlBufferAdd(buf, str, 1);
}

char * nd_extract_auth_val(xmlChar **q) {
	int quoted = 0;
	char * ret;
	xmlChar * qq = *q;
	xmlBufferPtr val = xmlBufferCreate();

	SKIP_BLANKS(qq);
	if (*qq == '"') {
		quoted = 1;
		++qq;
	}

	while (*qq != '\0') {
		if (quoted && *qq == '"') {
			++qq;
			break;
		}
		if ( ! quoted) {
			if ( strchr("()<>@,;:\\\"/?= \t", *qq) != NULL ) {
				++qq;
				break;
			} else if ( (*qq < 037) || (*qq == 0177) ) {
					++qq;
					break;
			}
		} else if (quoted && *qq == '\\')
			xmlBufferAddChar(val, *qq++);

		xmlBufferAddChar(val, *qq++);
	}; /* while */

	if (*qq != '\0') {
		SKIP_BLANKS(qq);
		if (*qq == ',')
			++qq;
	}
	*q = qq;
	ret = (char *) xmlBufferContent(val);
	xmlFree(val);
	return ret;
}; /* nd_extract_auth_val(char **) */

ndAuthParamPtr ndAuthParamCreate(struct http_auth * hauth, xmlChar * p) {
	ndAuthParamPtr param, ap;

	/* Init Param */
	if ( (param = hauth->param_fn()) == NULL )
		return NULL;

	while (*p != '\0') {
		SKIP_BLANKS(p);
		for (ap = param; ap->name != NULL; ap++) {
			if (strncasecmp((char *) p, ap->name, strlen(ap->name)) == 0) {
				p += strlen(ap->name);
				SKIP_BLANKS(p);

				if (*p != '=')
					return NULL;

				p++;
				ap->val = nd_extract_auth_val(&p);
				if (*p == ',')
					++p;
				break;
			}
		}
	}
	return param;
}; /* ndAuthParamCreate(...) */

ndAuthParamPtr ndAuthParamCreateBasic(void)
{
	ndAuthParamPtr param = xmlMalloc(sizeof(ndAuthParam) * 5);
	param[0].name	= "name";
	param[0].val	= xmlMemStrdup("Basic");
	param[1].name	= "realm";
	param[1].val	= NULL;
	param[2].name	= "user";
	param[2].val	= NULL;
	param[3].name	= "password";
	param[3].val	= NULL;
	param[4].name	= NULL;
	param[4].val	= NULL;

	return param;
}; /* ndAuthParamCreateBasic(void) */

void ndAuthParamFree(ndAuthParamPtr auth_param) {
	ndAuthParamPtr ap;

	for (ap = auth_param; ap->name != NULL; ap++) {
		if (ap->val != NULL)
			xmlFree(ap->val);
	}

	xmlFree(auth_param);
}; /* ndAuthParamFree(ndAuthParamPtr) */

char * ndAuthParamValue(ndAuthParamPtr param, char *name) {
	ndAuthParamPtr ap;

	for (ap = param; ap->name != NULL; ap++)
		if (! strcmp(ap->name, name))
			return ap->val;

	return NULL;
}; /* ndAuthParamValue(ndAuthParamPtr, char *) */

int ndAuthParamSetValue(ndAuthParamPtr param, char *name, char *val) {
	ndAuthParamPtr ap;

	for (ap = param; ap->name != NULL; ap++)
		if (! strcmp(ap->name, name)) {
			if (ap->val != NULL)
				xmlFree(ap->val);
			ap->val = xmlMemStrdup(val);
			return 0;

		}

	return -1;
}; /* ndAuthParamSetValue(ndAuthParamPtr, char *, char *) */

ndAuthParamPtr ndAuthParamCreateDigest(void) {
	ndAuthParamPtr param;

#if USE_DIGEST && HAVE_MHASH_H
	char timestring[12];
	xmlBufferPtr cnonce = xmlBufferCreate();

	param = xmlMalloc(sizeof(ndAuthParam) * 13);

	param[0].name	= "name";
	param[0].val	= xmlMemStrdup("Digest");
	param[1].name	= "realm";
	param[1].val	= NULL;
	param[2].name  = "domain";
	param[2].val   = NULL;
	param[3].name  = "nonce";
	param[3].val   = NULL;
	param[4].name  = "opaque";
	param[4].val   = NULL;
	param[5].name  = "stale";
	param[5].val   = NULL;
	param[6].name  = "algorithm";
	param[6].val   = NULL;
	param[7].name  = "qop";
	param[7].val   = NULL;
	param[8].name	= "user";
	param[8].val	= NULL;
	param[9].name  = "password";
	param[9].val   = NULL;
	param[10].name	= "uri";
	param[10].val	= NULL;

	param[11].name  = "cnonce";
#ifdef HAVE_TIME
	snprintf(timestring, sizeof(timestring), "%u", (unsigned int) time(0));
	xmlBufferAdd(cnonce, (xmlChar *) timestring, -1);
#endif
	xmlBufferAdd(cnonce, (xmlChar *) ":197e5d7fa:nd", -1);
	param[11].val   = (char *) xmlBufferContent(ndAuthEncodeDigest(cnonce));
	xmlBufferFree (cnonce);

	param[12].name  = NULL;
	param[12].val   = NULL;
#else
	/* Not yet implemented */
	fprintf(stderr, "Digest authentication is not implemented.\n");
	param = NULL;
#endif

	return param;
}; /* ndAuthParamCreateDigest(void) */

xmlBufferPtr ndAuthEncodeBasic(char *a) {
	unsigned char d[3];
	unsigned char c1, c2, c3, c4;
	int i, n_pad;
	xmlBufferPtr w = xmlBufferCreate();

	while (1) {
		if (*a == '\0')
			break;

		n_pad = 0;
		d[1] = d[2] = 0;

		for (i = 0; i < 3; i++) {
			d[i] = a[i];
			if (a[i] == '\0') {
				n_pad = 3 - i;
				break;
			}
		}

		c1 = d[0] >> 2;
		c2 = ( ( (d[0] << 4) | (d[1] >> 4) ) & 0x3f );

		if (n_pad == 2)
			c3 = c4 = 64;
	
		else if (n_pad == 1) {
				c3 = ( (d[1] << 2) & 0x3f );
				c4 = 64;
		} else {
				c3 = (((d[1] << 2) | (d[2] >> 6)) & 0x3f);
				c4 = (d[2] & 0x3f);
		}

		xmlBufferAddChar(w, Base64Table[c1]);
		xmlBufferAddChar(w, Base64Table[c2]);
		xmlBufferAddChar(w, Base64Table[c3]);
		xmlBufferAddChar(w, Base64Table[c4]);

		if (n_pad)
			break;
		
		a += 3;
	}; /* while(1) */

	return w;
}; /* ndAuthEncodeBasic(char *) */

xmlBufferPtr ndAuthEncodeDigest(xmlBufferPtr a) {
#if HAVE_MHASH_H && USE_DIGEST
	unsigned int j;
	char octet[3];
	unsigned char hash[16];
	MHASH context;
	xmlBufferPtr w = xmlBufferCreate();

	context = mhash_init(MHASH_MD5);
	mhash(context, (char *) xmlBufferContent(a), xmlBufferLength(a));
	mhash_deinit(context, hash);
	for (j = 0; j < sizeof(hash); ++j) {
		snprintf(octet, sizeof(octet), "%02x", hash[j]);
		xmlBufferAdd(w, (xmlChar *) octet, strlen(octet));
	}

	return w;
#else
	return NULL;
#endif /* HAVE_MHASH_H && USE_DIGEST */
} /* ndAuthEncodeDigest(xmlBufferPtr) */

xmlBufferPtr ndAuthBasic(ndAuthParamPtr param) {
	xmlBufferPtr buf = xmlBufferCreate();

	xmlBufferAdd(buf, (xmlChar *) ndAuthParamValue(param, "user"), -1);
	xmlBufferAdd(buf, (xmlChar *) ":", -1);
	xmlBufferAdd(buf, (xmlChar *) ndAuthParamValue(param, "password"), -1);

	return ndAuthEncodeBasic((char *) xmlBufferContent(buf));
}; /* ndAuthBasic(ndAuthParamPtr) */

#if USE_DIGEST && HAVE_MHASH_H
void add_quoted_param(xmlBufferPtr buf, ndAuthParamPtr param,
		const char * name_str, char * param_name, int flag) {
	if (flag)
		xmlBufferAdd(buf, (xmlChar *) ", ", -1);
	
	xmlBufferAdd(buf, (xmlChar *) name_str, -1);
	xmlBufferAdd(buf, (xmlChar *) "=\"", -1);
	xmlBufferAdd(buf, (xmlChar *) ndAuthParamValue(param, param_name), -1);
	xmlBufferAdd(buf, (xmlChar *) "\"", -1);
} /* add_quoted_param(xmlBufferPtr, ndAuthParamPtr) */
#endif

xmlBufferPtr ndAuthDigest(ndAuthParamPtr param) {
#if USE_DIGEST && HAVE_MHASH_H
	char nc_string[9];
	xmlBufferPtr buf1 = xmlBufferCreate();
	xmlBufferPtr A1 = xmlBufferCreate();
	xmlBufferPtr A2 = xmlBufferCreate();
	xmlBufferPtr response;

	/* Identify the authenticated user. */
	xmlBufferAdd(A1, (xmlChar *) ndAuthParamValue(param, "user"), -1);
	xmlBufferAdd(A1, (xmlChar *) ":", -1);
	xmlBufferAdd(A1, (xmlChar *) ndAuthParamValue(param, "realm"), -1);
	xmlBufferAdd(A1, (xmlChar *) ":", -1);
	xmlBufferAdd(A1, (xmlChar *) ndAuthParamValue(param, "password"), -1);

	response = ndAuthEncodeDigest(A1);

	xmlBufferAdd(A2, (xmlChar *) export_method, -1);
	xmlBufferAdd(A2, (xmlChar *) ":", -1);
	xmlBufferAdd(A2, (xmlChar *) export_uri, -1);

	add_quoted_param(buf1, param, "username", "user", 0);

	add_quoted_param(buf1, param, "realm", "realm", 1);

	add_quoted_param(buf1, param, "nonce", "nonce", 1);

	xmlBufferAdd(response, (xmlChar *) ":", -1);
	xmlBufferAdd(response, (xmlChar *) ndAuthParamValue(param, "nonce"), -1);

	/* Check if 'nonce' has changed. If so, increment 'nonce_count'. */
	if ( strncmp(nonce_sensor, ndAuthParamValue(param, "nonce"),
				sizeof(nonce_sensor)) ) {
		++nc;
		strncpy(nonce_sensor, ndAuthParamValue(param, "nonce"),
				sizeof(nonce_sensor));
	}

	xmlBufferAdd(buf1, (xmlChar *) ", uri=\"", -1);
	xmlBufferAdd(buf1, (xmlChar *) export_uri, -1);
	xmlBufferAdd(buf1, (xmlChar *) "\"", -1);

	xmlBufferAdd(buf1, (xmlChar *) ", algorithm=\"MD5\"", -1);

	/* Use the new nonce_counter. */
	snprintf(nc_string, sizeof(nc_string), "%08x", nc);
	xmlBufferAdd(buf1, (xmlChar *) ", qop=\"auth\", nc=", -1);
	xmlBufferAdd(response, (xmlChar *) ":", -1);

	xmlBufferAdd(buf1, (xmlChar *) nc_string, -1);
	xmlBufferAdd(response, (xmlChar *) nc_string, -1);

	add_quoted_param(buf1, param, "cnonce", "cnonce", 1);

	xmlBufferAdd(response, (xmlChar *) ":", -1);
	xmlBufferAdd(response, (xmlChar *) ndAuthParamValue(param, "cnonce"), -1);
	xmlBufferAdd(response, (xmlChar *) ":auth:", -1);
	xmlBufferAdd(response, xmlBufferContent(ndAuthEncodeDigest(A2)), -1);

	xmlBufferFree(A2); /* Used inside 'response'. */

	xmlBufferAdd(buf1, (xmlChar *) ", response=\"", -1);
	xmlBufferAdd(buf1, xmlBufferContent(ndAuthEncodeDigest(response)), -1);
	xmlBufferAdd(buf1, (xmlChar *) "\"", -1);
	xmlBufferFree(response);

	if (ndAuthParamValue(param, "opaque"))
		add_quoted_param(buf1, param, "opaque", "opaque", 1);

	return buf1;
#else
	return NULL;
#endif
}; /* ndAuthDigest(ndAuthPtr) */

int ndAuthCreateHeader(char *str, ndAuthCallback fn,
						 xmlBufferPtr *buf_return, int is_proxy) {
	xmlChar * p;
	struct http_auth * ha;
	struct http_auth * hauth = NULL;
	ndAuthParamPtr param = NULL;

	p = (xmlChar *) str;
	for (ha = &www_auth[0]; ha->name != NULL; ha++) {
		if ( strncasecmp((char *) p, ha->name, strlen(ha->name)) == 0 ) {
			hauth = ha;
			p += strlen(ha->name);
			SKIP_BLANKS(p);
		}
	}

	if (hauth != NULL)
		param = ndAuthParamCreate(hauth, p);

	if ( param && ! ( fn(param, is_proxy) ) ) {
		xmlBufferPtr tmp;
		xmlBufferPtr ret = xmlBufferCreate();

		xmlBufferAdd(ret,
					is_proxy ? (xmlChar *) "Proxy-Authorization:"
							 : (xmlChar *) "Authorization: ",
					-1);
		xmlBufferAdd(ret, (xmlChar *) ndAuthParamValue(param, "name"), -1);
		xmlBufferAdd(ret, (xmlChar *) " ", -1);
		tmp = hauth->auth_fn(param);
		ndAuthParamFree(param);
		xmlBufferAdd(ret, xmlBufferContent(tmp), -1);
		xmlBufferAdd(ret, (xmlChar *) "\r\n", -1);
		*buf_return = ret;
		return 0;
	}

	return -1;
}; /* ndAuthCreateHeader(char *, ndAuthCallback, xmlBufferPtr *, int) */

ndAuthCtxtPtr
ndCreateAuthCtxt(ndAuthCallback auth_cb, ndAuthNotifyCallback notify_cb,
			 char *auth_realm, char *pauth_realm)
{
	ndAuthCtxtPtr auth;

	if ( (auth = xmlMalloc(sizeof(ndAuthCtxt))) == NULL )
		return NULL;

	memset(auth, 0, sizeof(ndAuthCtxt));
	auth->auth_cb = auth_cb;
	auth->notify_cb = notify_cb;
	auth->auth_realm = auth_realm;
	auth->pauth_realm = pauth_realm;

	return auth;
}; /* ndCreateAuthCtxt(...) */

void ndFreeAuthCtxt(ndAuthCtxtPtr auth) {
	if (auth == NULL)
		return;

	xmlFree(auth);
}; /* ndFreeAuthCtxt(ndAuthCtxtPtr) */
