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
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#define _GNU_SOURCE
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#include "ndav.h"
#include <libxml/tree.h>

#define SKIP_BLANKS(p) { while (*(p)&&isspace(*(p))) (p)++; }

/* Reused from mimehead.c */
const static char Base64Table[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

struct http_auth {
	char * name;
	ndAuthParamPtr (* param_fn)(void);
	xmlBufferPtr (*auth_fn)(ndAuthParamPtr param);
};

ndAuthParamPtr ndAuthParamCreateBasic();
ndAuthParamPtr ndAuthParamCreateDigest();

xmlBufferPtr ndAuthBasic(ndAuthParamPtr param);
xmlBufferPtr ndAuthDigest(ndAuthParamPtr param);

struct http_auth www_auth[] =
{
		{"Basic",	ndAuthParamCreateBasic, ndAuthBasic},
		{"Digest",	ndAuthParamCreateDigest, ndAuthDigest},
		{NULL, NULL}
};

/* Shorhand operator to add a singel character. */
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
			xmlBufferAddChar(val, *qq++);
		}

		while (*qq != '\0') {
			if (quoted && *qq == '"') {
				xmlBufferAddChar(val, *qq++);
				break;
			}
			if ( ! quoted) {
				if ( strchr("()<>@,;:\\\"/?= \t", *qq) != NULL ) {
					q++;
					break;
				} else
					if ( (*qq < 037) || (*qq == 0177) ) {
						q++;
						break;
					}
			} else
				if (quoted && *qq == '\\')
					xmlBufferAddChar(val, *qq++);

			xmlBufferAddChar(val, *qq++);
		}; /* while */

		if (*qq != '\0') {
			SKIP_BLANKS(qq);
			if (*qq == ',')
				qq++;
		}
		*q = qq;
		ret = (char *) xmlBufferContent(val);
		xmlFree(val);
		return ret;
}; /* nd_extract_auth_val(char **) */

ndAuthParamPtr ndAuthParamCreate(struct http_auth * hauth, xmlChar * p) {
	ndAuthParamPtr param, ap;

	/* Init Param */
	param = hauth->param_fn();

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
	/* Not yet implemented */
	return NULL;
}; /* ndAuthParamCreateDigest(void) */

xmlBufferPtr ndAuthEncodeB(char *a) {
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
	
			else
				if (n_pad == 1) {
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
}; /* ndAuthEncodeB(char *) */

xmlBufferPtr ndAuthBasic(ndAuthParamPtr param) {
	xmlBufferPtr buf = xmlBufferCreate();

	xmlBufferAdd(buf, (xmlChar *) ndAuthParamValue(param, "user"), -1);
	xmlBufferAdd(buf, (xmlChar *) ":", -1);
	xmlBufferAdd(buf, (xmlChar *) ndAuthParamValue(param, "password"), -1);

	return ndAuthEncodeB((char *) xmlBufferContent(buf));
}; /* ndAuthBasic(ndAuthParamPtr) */

xmlBufferPtr ndAuthDigest(ndAuthParamPtr param) {
	return NULL;
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
ndCreateAuthCtxt( ndAuthCallback auth_cb, ndAuthNotifyCallback notify_cb,
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
