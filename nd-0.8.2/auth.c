/*
 * auth.c - authentication module for nd.
 *
 * Copyright (c) 2002 Yuuichi Teranishi <teranisi@gohome.org>
 * For license terms, see the file COPYING in this directory.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include <ctype.h>

#include "nd.h"
#include <libxml/tree.h>

#define SKIP_BLANKS(p) {while(*(p)&&isspace(*(p)))(p)++;}

struct http_auth
{
  char            *name;
  ndAuthParamPtr (*param_fn) (void);
  xmlBufferPtr   (*auth_fn) (ndAuthParamPtr param);
};

ndAuthParamPtr ndAuthParamCreateBasic ();
ndAuthParamPtr ndAuthParamCreateDigest ();
xmlBufferPtr ndAuthBasic (ndAuthParamPtr param);
xmlBufferPtr ndAuthDigest (ndAuthParamPtr param);

struct http_auth www_auth [] =
  {
    {"Basic", ndAuthParamCreateBasic, ndAuthBasic},
    {"Digest", ndAuthParamCreateDigest, ndAuthDigest},
    {NULL, NULL}
  };

/* util for convinience */
void
xmlBufferAddChar (buf, ch)
     xmlBufferPtr buf;
     char ch;
{
  char str[2];
  str [0] = ch;
  str [1] = '\0';
  xmlBufferAdd(buf, str, 1);
}

char *
nd_extract_auth_val (char **q)
{
    unsigned char *qq = *(unsigned char **)q;
    int quoted = 0;
    xmlBufferPtr val = xmlBufferCreate ();
    char *ret;

    SKIP_BLANKS(qq);
    if (*qq == '"')
      {
	quoted = 1;
	xmlBufferAddChar (val, *qq++);
      }
    while (*qq != '\0')
      {
	if (quoted && *qq == '"')
	  {
	    xmlBufferAddChar (val, *qq++);
	    break;
	  }
	if (!quoted)
	  {
	    switch (*qq)
	      {
	      case '(':
	      case ')':
	      case '<':
	      case '>':
	      case '@':
	      case ',':
	      case ';':
	      case ':':
	      case '\\':
	      case '"':
	      case '/':
	      case '?':
	      case '=':
	      case ' ':
	      case '\t':
		qq++;
		goto end_token;
	      default:
		if (*qq <= 037 || *qq == 177)
		  {
		    qq++;
		    goto end_token;
		  }
	      }	
	  }
	else if (quoted && *qq == '\\')
	  xmlBufferAddChar (val, *qq++);
	xmlBufferAddChar (val, *qq++);
      }
 end_token:
    if (*qq != '\0')
      {
	SKIP_BLANKS(qq);
	if (*qq == ',')
	  qq++;
      }
    *q = qq;
    ret = (char *) xmlBufferContent(val);
    xmlFree (val);
    return ret;
}

ndAuthParamPtr
ndAuthParamCreate (hauth, p)
     struct http_auth *hauth;
     char *p;
{
  ndAuthParamPtr param, ap;

  /* Init Param */
  param = hauth->param_fn ();
  while (*p != '\0') {
    SKIP_BLANKS(p);
    for (ap = param; ap->name != NULL; ap++)
      {
	if (strncasecmp(p, ap->name, strlen(ap->name)) == 0)
	  {
	    p += strlen (ap->name);
	    SKIP_BLANKS(p);
	    if (*p != '=')
	      return NULL;
	    p++;
	    ap->val = nd_extract_auth_val (&p);
	    break;
	  }
      }
  }
  return param;
}

ndAuthParamPtr
ndAuthParamCreateBasic ()
{
  ndAuthParamPtr param = xmlMalloc (sizeof (ndAuthParam) * 5);

  param[0].name  = "name";
  param[0].val   = xmlMemStrdup ("Basic");
  param[1].name  = "realm";
  param[1].val   = NULL;
  param[2].name  = "user";
  param[2].val   = NULL;
  param[3].name  = "password";
  param[3].val   = NULL;
  param[4].name  = NULL;
  param[4].val   = NULL;

  return param;
}

void
ndAuthParamFree (auth_param)
     ndAuthParamPtr auth_param;
{
  ndAuthParamPtr ap;
  for (ap = auth_param; ap->name != NULL; ap++)
    {
      if (ap->val != NULL)
	xmlFree (ap->val);
    }
  xmlFree (auth_param);
}

char *
ndAuthParamValue (param, name)
     ndAuthParamPtr param;
     char *name;
{
  ndAuthParamPtr ap;
  for (ap = param; ap->name != NULL; ap++)
    {
      if (!strcmp (ap->name, name))
	return ap->val;
    }
  return NULL;
}

int
ndAuthParamSetValue (param, name, val)
     ndAuthParamPtr param;
     char *name;
     char *val;
{
  ndAuthParamPtr ap;
  for (ap = param; ap->name != NULL; ap++)
    {
      if (!strcmp (ap->name, name))
	{
	  if (ap->val != NULL)
	    xmlFree (ap->val);
	  ap->val = xmlMemStrdup (val);
	  return 0;
	}
    }
  return -1;
}

ndAuthParamPtr
ndAuthParamCreateDigest ()
{
  /* Not Implemented */
  return NULL;
}

/* Delived from mimehead.c */
static char Base64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

xmlBufferPtr
ndAuthEncodeB(char *a)
{
    unsigned char d[3];
    unsigned char c1, c2, c3, c4;
    int i, n_pad;
    xmlBufferPtr w = xmlBufferCreate ();

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
	c2 = (((d[0] << 4) | (d[1] >> 4)) & 0x3f);
	if (n_pad == 2) {
	    c3 = c4 = 64;
	}
	else if (n_pad == 1) {
	    c3 = ((d[1] << 2) & 0x3f);
	    c4 = 64;
	}
	else {
	    c3 = (((d[1] << 2) | (d[2] >> 6)) & 0x3f);
	    c4 = (d[2] & 0x3f);
	}
	xmlBufferAddChar (w, Base64Table[c1]);
	xmlBufferAddChar (w, Base64Table[c2]);
	xmlBufferAddChar (w, Base64Table[c3]);
	xmlBufferAddChar (w, Base64Table[c4]);
	if (n_pad)
	    break;
	a += 3;
    }
    return w;
}

xmlBufferPtr
ndAuthBasic (param)
     ndAuthParamPtr param;
{
  xmlBufferPtr buf = xmlBufferCreate ();
  xmlBufferAdd (buf, ndAuthParamValue (param, "user"), -1);
  xmlBufferAdd (buf, ":", -1);
  xmlBufferAdd (buf, ndAuthParamValue (param, "password"), -1);
  return ndAuthEncodeB ((char *) xmlBufferContent (buf));
}

xmlBufferPtr
ndAuthDigest (param)
     ndAuthParamPtr param;
{
  return NULL;
}

int
ndAuthCreateHeader (str, fn, buf_return, is_proxy)
     char *str;
     ndAuthCallback fn;
     xmlBufferPtr *buf_return;
     int is_proxy;
{
  char *p;
  struct http_auth *ha;
  struct http_auth *hauth = NULL;
  ndAuthParamPtr param = NULL;

  p = str;
  for (ha = &www_auth[0]; ha->name != NULL; ha++)
    {
      if (strncasecmp (p, ha->name, strlen (ha->name)) == 0)
	{
	  hauth = ha;
	  p += strlen (ha->name);
	  SKIP_BLANKS(p);
	}
    }
  if (hauth != NULL)
    {
      param = ndAuthParamCreate (hauth, p);
    }
  if (param && !(fn (param, is_proxy)))
    {
      xmlBufferPtr tmp;
      xmlBufferPtr ret = xmlBufferCreate ();

      xmlBufferAdd (ret,
		    is_proxy? "Proxy-Authorization:" : "Authorization: ",
		    -1);
      xmlBufferAdd (ret, ndAuthParamValue (param, "name"), -1);
      xmlBufferAdd (ret, " ", -1);
      tmp = hauth->auth_fn (param);
      ndAuthParamFree (param);
      xmlBufferAdd (ret, xmlBufferContent (tmp), -1);
      xmlBufferAdd (ret, "\r\n", -1);
      *buf_return = ret;
      return 0;
    }
  return -1;
}

ndAuthCtxtPtr
ndCreateAuthCtxt (auth_cb, notify_cb, auth_realm, pauth_realm)
     ndAuthCallback auth_cb;
     ndAuthNotifyCallback notify_cb;
     char *auth_realm;
     char *pauth_realm;
{
  ndAuthCtxtPtr auth = xmlMalloc (sizeof (ndAuthCtxt));
  if (auth == NULL) return NULL;
  memset (auth, 0, sizeof (ndAuthCtxt));
  auth->auth_cb = auth_cb;
  auth->notify_cb = notify_cb;
  auth->auth_realm = auth_realm;
  auth->pauth_realm = pauth_realm;
  return auth;
}

void
ndFreeAuthCtxt (auth)
     ndAuthCtxtPtr auth;
{
  if (auth == NULL) return;
  xmlFree (auth);
}
