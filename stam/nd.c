/*
 * nd.c - a small program for WebDAV operations.
 *
 * Copyright (c) 2002 Yuuichi Teranishi <teranisi@gohome.org>
 * For license terms, see the file COPYING in this directory.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <libxml/nanohttp.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>

#include "nd.h"

ndNodeInfoPtr
ndNodeInfoNew ()
{
  ndNodeInfoPtr ret = xmlMalloc (sizeof (ndNodeInfo));
  if (ret == NULL) return (NULL);
  memset(ret, 0, sizeof(ndNodeInfo));
  return(ret);
}

void
ndNodeInfoFree (info)
     ndNodeInfoPtr info;
{
  if (info == NULL) return;
  if (info->name) xmlFree(info->name);
  if (info->date) xmlFree(info->date);
  if (info->cdate) xmlFree(info->cdate);
  if (info->size) xmlFree(info->size);
  if (info->content) xmlFree(info->content);
  if (info->restype) xmlFree(info->restype);
  if (info->props) ndPropListFree(info->props);
  if (info->lock) ndLockInfoListFree (info->lock);
  xmlFree (info);
}

void
ndNodeInfoListFree (info)
     ndNodeInfoPtr info;
{
  ndNodeInfoPtr cur = info;

  while (cur != NULL)
    {
      info = info->next;
      ndNodeInfoFree(cur);
      cur = info;
    }
}

void
ndNodeInfoPrint(fp, info, format)
     FILE *fp;
     ndNodeInfoPtr info;
     int format;
{
  if (format == ND_PRINT_AS_HEADER)
    {
      if (info == NULL)
	return;
      fprintf (fp, "Name: %s\n", info->name? info->name: "");
      fprintf (fp, "Status: %d\n", info->status);
      fprintf (fp, "Last-Modified: %s\n", info->date? info->date: "");
      fprintf (fp, "Created: %s\n", info->cdate? info->cdate: "");
      fprintf (fp, "Size: %s\n", info->size? info->size: "");
      fprintf (fp, "Content-Type: %s\n", info->content? info->content: "");
      fprintf (fp, "Resource-Type: %s\n", info->restype? info->restype: "");
      if (info->props)
	{
	  ndPropListPrint (fp, info->props, format);
	}
      if (info->lock)
	{
	  ndLockInfoListPrint (fp, info->lock, format);
	}
      fprintf (fp, "\n");
    }
  else if (format == ND_PRINT_AS_SEXP)
    {
      if (info == NULL)
	return;
      fprintf (fp, "(\"%s\"", info->name? info->name: "");
      if (info->date)
	fprintf (fp, " (last-modified \"%s\")", info->date);
      if (info->cdate)
	fprintf (fp, " (created \"%s\")", info->cdate);
      if (info->status)
	fprintf (fp, " (status %d)", info->status);
      if (info->size)
	fprintf (fp, " (size %s)", info->size);
      if (info->content)
	fprintf (fp, " (content-type \"%s\")", info->content);
      if (info->restype)
	fprintf (fp, " (resourcetype \"%s\")", info->restype);
      if (info->props)
	{
	  fprintf (fp, " ");
	  ndPropListPrint (fp, info->props, format);
	}
      if (info->lock)
	{
	  fprintf (fp, " ");
	  ndLockInfoListPrint (fp, info->lock, format);
	}
      fprintf (fp, ")");
    }
}

void
ndNodeInfoListPrint (fp, info, format)
     FILE *fp;
     ndNodeInfoPtr info;
     int format;
{
  ndNodeInfoPtr cur = info;

  if (format == ND_PRINT_AS_SEXP)
    fprintf (fp, "(");
  while (cur != NULL)
    {
      info = info->next;
      ndNodeInfoPrint(fp, cur, format);
      cur = info;
      if (format == ND_PRINT_AS_SEXP && cur != NULL) fprintf (fp, " ");
    }
  if (format == ND_PRINT_AS_SEXP)
    fprintf (fp, ")\n");
}

ndLockInfoPtr
ndLockInfoNew ()
{
  ndLockInfoPtr ret = xmlMalloc (sizeof (ndLockInfo));
  if (ret == NULL) return (NULL);
  memset (ret, 0, sizeof(ndLockInfo));
  return (ret);
}

void
ndLockInfoFree (lock)
     ndLockInfoPtr lock;
{
  if (lock == NULL) return;
  if (lock->scope) xmlFree(lock->scope);
  if (lock->type) xmlFree(lock->type);
  if (lock->owner_href) xmlFree(lock->owner_href);
  if (lock->token) xmlFree(lock->token);
  if (lock->timeout) xmlFree(lock->timeout);
  xmlFree (lock);
}

void
ndLockInfoListFree (info)
     ndLockInfoPtr info;
{
  ndLockInfoPtr cur = info;

  while (cur != NULL)
    {
      info = info->next;
      ndLockInfoFree(cur);
      cur = info;
    }
}

void
ndLockInfoPrint (fp, lock, format)
     FILE *fp;
     ndLockInfoPtr lock;
     int format;
{
  if (format == ND_PRINT_AS_HEADER)
    {
      if (lock == NULL) return;
      fprintf (fp, "Lock: ");
      if (lock->token) fprintf (fp, "token=\"%s\",\n", lock->token);
      fprintf (fp, "      scope=\"%s\",\n", lock->scope? lock->scope : "");
      fprintf (fp, "      owner-href=\"%s\",\n",
	       lock->owner_href? lock->owner_href : "");
      fprintf (fp,
	       "      timeout=\"%s\"\n", lock->timeout? lock->timeout : "");
    }
  else if (format == ND_PRINT_AS_SEXP)
    {
      fprintf (fp, "(lock");
      if (lock->token) fprintf (fp, " (token \"%s\")", lock->token);
      fprintf (fp, " (scope %s)", lock->scope? lock->scope : "");
      fprintf (fp, " (owner-href \"%s\")",
	       lock->owner_href? lock->owner_href : "");
      fprintf (fp, " (timeout \"%s\"))",
	       lock->timeout? lock->timeout : "");
    }
}

void
ndLockInfoListPrint (fp, info, format)
     FILE *fp;
     ndLockInfoPtr info;
     int format;
{
  ndLockInfoPtr cur = info;

  if (format == ND_PRINT_AS_SEXP)
    fprintf (fp, "(lock-list ");
  while (cur != NULL)
    {
      info = info->next;
      ndLockInfoPrint(fp, cur, format);
      cur = info;
      if (format == ND_PRINT_AS_SEXP && cur != NULL) fprintf (fp, " ");
    }
  if (format == ND_PRINT_AS_SEXP)
    fprintf (fp, ")\n");
}

ndPropPtr
ndPropNew ()
{
  ndPropPtr ret = xmlMalloc (sizeof (ndProp));
  if (ret == NULL) return (NULL);
  memset (ret, 0, sizeof(ndProp));
  return (ret);
}

void
ndPropFree (prop)
     ndPropPtr prop;
{
  if (prop == NULL) return;
  if (prop->name) xmlFree(prop->name);
  if (prop->value) xmlFree(prop->value);
  if (prop->ns) xmlFree(prop->ns);
  xmlFree (prop);
}

void
ndPropListFree (prop)
     ndPropPtr prop;
{
  ndPropPtr cur = prop;

  while (cur != NULL)
    {
      prop = prop->next;
      ndPropFree(cur);
      cur = prop;
    }
}

void
ndPropPrint (fp, prop, format)
     FILE *fp;
     ndPropPtr prop;
     int format;
{
  if (format == ND_PRINT_AS_HEADER)
    {
      if (prop == NULL) return;
      if (prop->name)
	{
	  if (prop->value && *prop->value == '\"')
	    {
	      fprintf (fp, "Property: %s=%s", prop->name, prop->value);
	    }
	  else
	    {
	      if (prop->value)
		fprintf (fp, "Property: %s=\"%s\"", prop->name, prop->value);
	    }
	}
      if (prop->ns)
	{
	  fprintf (fp, "; ns=\"%s\"\n", prop->ns);
	}
      else
	{
	  fprintf (fp, "\n");
	}
    }
  else if (format == ND_PRINT_AS_SEXP)
    {
      fprintf (fp, "(property");
      fprintf (fp, " (name \"%s\")", prop->name? prop->name : "");
      if (prop->value && *prop->value == '\"')
	{
	  fprintf (fp, " (value %s)", prop->value);
	}
      else
	{
	  fprintf (fp, " (value \"%s\")", prop->value? prop->value : "");
	}
      if (prop->ns)
	fprintf (fp, " (ns \"%s\")", prop->ns);
      fprintf (fp, ")");
    }
}

void
ndPropListPrint (fp, prop, format)
     FILE *fp;
     ndPropPtr prop;
     int format;
{
  ndPropPtr cur = prop;

  while (cur != NULL)
    {
      prop = prop->next;
      ndPropPrint(fp, cur, format);
      cur = prop;
      if (format == ND_PRINT_AS_SEXP && cur != NULL) fprintf (fp, " ");
    }
}

void*
ndHTTPMethod (URL, auth, method, input, contentType, headers, ilen)
     const char *URL;
     ndAuthCtxtPtr auth;
     const char *method;
     const char *input;
     char **contentType;
     const char *headers;
     int ilen;
{
  void *ctxt;
  int returnCode;
  char *auth_header = NULL;
  char *proxy_auth_header = NULL;
  xmlBufferPtr header_buf = NULL;
  char line [ND_HEADER_LINE_MAX];
  xmlBufferPtr temp_buf = NULL;

  header_buf = xmlBufferCreate ();
  if (header_buf == NULL) return NULL;

  if (auth && (auth->pauth_realm))
    {
      if (snprintf (line, ND_HEADER_LINE_MAX,
		    "Basic realm=\"%s\"", auth->pauth_realm) >=
	  ND_HEADER_LINE_MAX)
	{
	  return NULL;
	}
      if (ndAuthCreateHeader (line, auth->auth_cb, &temp_buf, 1) < 0)
	return NULL;
      if (temp_buf == NULL) return NULL;
      xmlBufferAdd (header_buf, xmlBufferContent (temp_buf), -1);
      xmlBufferFree (temp_buf);
    }
  if (auth && (auth->auth_realm))
    {
      if (snprintf (line, ND_HEADER_LINE_MAX,
		    "Basic realm=\"%s\"", auth->auth_realm) >=
	  ND_HEADER_LINE_MAX)
	{
	  return NULL;
	}
      if (ndAuthCreateHeader (line, auth->auth_cb, &temp_buf, 0) < 0)
	return NULL;
      if (temp_buf == NULL) return NULL;
      xmlBufferAdd (header_buf, xmlBufferContent (temp_buf), -1);
      xmlBufferFree (temp_buf);
    }

  if (headers) xmlBufferAdd (header_buf, headers, -1);

  ctxt = xmlNanoHTTPMethod (URL, method, input, contentType,
			    xmlBufferLength (header_buf) == 0 ?
			    NULL : xmlBufferContent (header_buf), ilen);
  xmlBufferFree (header_buf);

  if (ctxt == NULL) return(NULL);
  returnCode = xmlNanoHTTPReturnCode(ctxt);
  while (returnCode == 401 || returnCode == 407)
    {
      auth_header = (char *) xmlNanoHTTPAuthHeader (ctxt);
      if (!auth || auth->auth_cb == NULL)
	return ctxt;
      if (ndAuthCreateHeader (auth_header, auth->auth_cb, &header_buf,
			      (returnCode == 407)) < 0)
	return NULL;
      if (header_buf == NULL) return (NULL);
      if (returnCode == 407)
	{
	  if (proxy_auth_header) xmlFree (proxy_auth_header);
	  proxy_auth_header = xmlMemStrdup (xmlBufferContent (header_buf));
	  xmlBufferFree (header_buf);
	  header_buf = xmlBufferCreate ();
	}
      if (headers)
	xmlBufferAdd (header_buf, headers, -1);
      if (proxy_auth_header)
	xmlBufferAdd (header_buf, proxy_auth_header, -1);
      /* XXX If HTTP/1.1 and Keep-Alive, no need to close here. */
      xmlNanoHTTPClose (ctxt);
      ctxt = xmlNanoHTTPMethod(URL, method, input, contentType,
			       xmlBufferContent (header_buf), ilen);
      xmlBufferFree (header_buf);
      if (ctxt == NULL) return(NULL);
      returnCode = xmlNanoHTTPReturnCode(ctxt);
    }
  if (proxy_auth_header) xmlFree (proxy_auth_header);
  if (auth && auth->notify_cb) auth->notify_cb (ctxt);
  return ctxt;
}


/* Generic DAV request function. */
int
nd_dav_request (method, url, auth, header, content, length,
		buf_return)
     char *method;
     char *url;
     ndAuthCtxtPtr auth;
     char *header;
     char *content;
     int length;
     xmlBufferPtr *buf_return;
{
  void *ctxt;
  char *contentType = "text/xml; charset=\"utf-8\"";
  int len;
  xmlChar buffer [1024];
  xmlBufferPtr output;
  int returnCode;

  if (url == NULL) return -1;

  ctxt = ndHTTPMethod(url, auth,
		      method, content, &contentType, header,
		      content? strlen (content): 0);
  if (ctxt == NULL)
    {
      return -1;
    }
  returnCode = xmlNanoHTTPReturnCode(ctxt);
  if ((returnCode >= 300) ||
      ((contentType != NULL) && (strncmp(contentType, "text/xml", 8)))) {
    xmlNanoHTTPClose(ctxt);
    if (contentType != NULL) xmlFree(contentType);
    return returnCode;
  }
  if (contentType != NULL) xmlFree(contentType);

  output = xmlBufferCreate ();
  if (output == NULL) return -1;

  while ( (len = xmlNanoHTTPRead (ctxt, buffer, sizeof (buffer))) > 0)
    {
      xmlBufferAdd (output, buffer, len);
    }
  xmlNanoHTTPClose(ctxt);
  *buf_return = output;
  return returnCode;
}

/*
 * PROPFIND(ALLPROP)
 */
int
nd_propfind_all_query (url, auth, depth, buf_return)
     char *url;
     ndAuthCtxtPtr auth;
     int depth;
     xmlBufferPtr *buf_return;
{
  char *verboseQuery =
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
<D:propfind xmlns:D=\"DAV:\">\
    <D:allprop/>\
</D:propfind>";
  char depth_header [ND_HEADER_LINE_MAX];
  char *depth_str;

  switch (depth)
    {
    case ND_DEPTH_0:
      depth_str = "0";
      break;
    case ND_DEPTH_1:
      depth_str = "1";
      break;
    case ND_DEPTH_INFINITE:
      depth_str = "Infinity";
      break;
    }
  sprintf (depth_header, "Depth: %s\r\n", depth_str);
  return nd_dav_request ("PROPFIND", url, auth,
			 depth_header, verboseQuery,
			 strlen (verboseQuery), buf_return);
  /*
  if (url[strlen(url) - 1] == '/')
    return nd_dav_request ("PROPFIND", url, fn, "Depth: 1\r\n", NULL, 0,
			   buf_return);
  else
    return nd_dav_request ("PROPFIND", url, fn, "Depth: 0\r\n", verboseQuery,
			   strlen (verboseQuery),
			   buf_return);
  */
}

/*
 * PROPFIND
 */
int
nd_propfind_query (url, auth, prop, ns, depth, buf_return)
     char *url;
     ndAuthCtxtPtr auth;
     char *prop;
     char *ns;
     int depth;
     xmlBufferPtr *buf_return;
{
  char depth_header [ND_HEADER_LINE_MAX];
  char propfind_request [ND_REQUEST_MAX];
  char *depth_str;

  if (ns == NULL)
    {
      ns = "DAV:";
    }
  switch (depth)
    {
    case ND_DEPTH_0:
      depth_str = "0";
      break;
    case ND_DEPTH_1:
      depth_str = "1";
      break;
    case ND_DEPTH_INFINITE:
      depth_str = "Infinity";
      break;
    }
  sprintf (depth_header, "Depth: %s\r\n", depth_str);
  if (snprintf (propfind_request, ND_REQUEST_MAX,
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
<D:propfind xmlns:D=\"DAV:\">\
  <D:prop xmlns:ns=\"%s\"><ns:%s/></D:prop>\
</D:propfind>", ns, prop) >= ND_REQUEST_MAX)
    {
      return -1;
    }
  return nd_dav_request ("PROPFIND", url, auth,
			 depth_header, propfind_request,
			 strlen (propfind_request), buf_return);
}

int
nd_dav_name_equal (node, name)
     xmlNodePtr node;
     char *name;
{
  if (node->ns && node->ns->href && (!strcmp (node->ns->href, "DAV:")))
    return !strcmp ((char *) node->name, name);
  return 0;
}

ndLockInfoPtr
nd_parse_activelock (cur)
     xmlNodePtr cur;
{
  ndLockInfoPtr ret = NULL;

  ret = ndLockInfoNew ();
  cur = cur->children;

  /*
   * activelock (lockscpoe, locktype, depth, owner?, timeout? locktoken?)
   */
  while (cur != NULL)
    {
      if (nd_dav_name_equal (cur, "lockscope"))
	{
	  if (cur->children != NULL)
	    {
	      ret->scope = xmlMemStrdup ((char *) cur->children->name);
	    }
	}
      else if (nd_dav_name_equal (cur, "owner"))
	{
	  if (cur->children != NULL)
	    {
	      if (nd_dav_name_equal (cur->children, "href"))
		ret->owner_href = (char *) xmlNodeGetContent (cur->children);
	    }
	}
      else if (nd_dav_name_equal (cur, "locktype"))
	{
	  ret->type = (char *) xmlNodeGetContent(cur);
	}
      else if (nd_dav_name_equal (cur, "locktoken"))
	{
	  char *curp, *start;
	  char *tmp = (char *) xmlNodeGetContent(cur);
	  if (tmp)
	    {
	      curp = tmp;
	      while (isspace(*curp))
		{
		  curp++;
		}
	      start = curp;
	      while (!isspace(*curp))
		{
		  curp++;
		}
	      if (*curp != '\0')
		*curp = '\0';
	      ret->token = xmlMemStrdup (start);
	      xmlFree (tmp);
	    }
	}
      else if (nd_dav_name_equal (cur, "depth"))
	{
	  ret->depth = (char *) xmlNodeGetContent(cur);
	}
      else if (nd_dav_name_equal (cur, "timeout"))
	{
	  ret->timeout = (char *) xmlNodeGetContent(cur);
	}
      cur = cur->next;
    }
  return(ret);
}

ndLockInfoPtr
nd_parse_lockdiscovery (cur)
     xmlNodePtr cur;
{
  ndLockInfoPtr ret, last, res;

  ret = last = NULL;
  cur = cur->children;
  while (cur != NULL)
    {
      if (nd_dav_name_equal (cur, "activelock"))
	{
	  res = nd_parse_activelock (cur);
	  if (res != NULL)
	    {
	      if (ret == NULL) ret = last = res;
	      else last->next = res;
	      while (last->next != NULL) last = last->next;
	    }
	}
      cur = cur->next;
    }
  return ret;
}

void
nd_parse_prop (cur, node)
     xmlNodePtr cur;
     ndNodeInfoPtr node;
{
  cur = cur->children;
  /*
   * prop ANY
   */
  while (cur != NULL)
    {
      if (nd_dav_name_equal (cur, "lockdiscovery"))
	{
	  node->lock = nd_parse_lockdiscovery (cur);
	}
      else if (nd_dav_name_equal (cur, "supportedlock"))
	{
	  // not supported yet.
	}
      else if (nd_dav_name_equal (cur, "getetag"))
	{
	  // not supported yet.
	}
      else if (nd_dav_name_equal (cur, "getlastmodified"))
	{
	  node->date = (char *) xmlNodeGetContent(cur);
	}
      else if (nd_dav_name_equal (cur, "creationdate"))
	{
	  node->cdate = (char *) xmlNodeGetContent(cur);
	}
      else if (nd_dav_name_equal (cur, "getcontenttype"))
	{
	  node->content = (char *) xmlNodeGetContent(cur);
	}
      else if (nd_dav_name_equal (cur, "getcontentlength"))
	{
	  node->size = (char *) xmlNodeGetContent(cur);
	}
      else if (nd_dav_name_equal (cur, "resourcetype"))
	{
	  if (cur->children != NULL)
	    {
	      node->restype = xmlMemStrdup ((char *) cur->children->name);
	    }
	}
      else if (strcmp(cur->name,"text")) /* XXX is this true? */
	{
	  ndPropPtr prop;
	  prop = node->props;
	  node->props = ndPropNew();
	  node->props->name = xmlMemStrdup((char *) cur->name);
	  node->props->value = (char *) xmlNodeGetContent(cur);
	  if (cur->ns && cur->ns->href && strcmp(cur->ns->href, "DAV:"))
	    {
	      node->props->ns = xmlMemStrdup((char *) cur->ns->href);
	    }
	  // ndPropPrint (stderr, node->props, ND_PRINT_AS_SEXP);
	  node->props->next = prop;
	}
      cur = cur->next;
    }
}

int
nd_parse_propstat (cur, node)
     xmlNodePtr cur;
     ndNodeInfoPtr node;
{
  int code = -1;
  cur = cur->children;

  /*
   * propstat (prop, status, responsedescription?)
   */
  while (cur != NULL)
    {
      if (nd_dav_name_equal (cur, "prop"))
	{
	  nd_parse_prop (cur, node);
	}
      else if (nd_dav_name_equal (cur, "status"))
	{
	  /* Property status */
	  char *status = (char *) xmlNodeGetContent (cur);
	  char *cur = status;
	  if (!strncmp(status, "HTTP/", 5))
	    {
	      int version = 0;
	      int ret = 0;

	      cur += 5;
	      while ((*cur >= '0') && (*cur <= '9')) {
		version *= 10;
		version += *cur - '0';
		cur++;
	      }
	      if (*cur == '.') {
		cur++;
		if ((*cur >= '0') && (*cur <= '9')) {
		  version *= 10;
		  version += *cur - '0';
		  cur++;
		}
		while ((*cur >= '0') && (*cur <= '9'))
		  cur++;
	      } else
		version *= 10;
	      if ((*cur != ' ') && (*cur != '\t')) return -1;
	      while ((*cur == ' ') || (*cur == '\t')) cur++;
	      if ((*cur < '0') || (*cur > '9')) return -1;
	      while ((*cur >= '0') && (*cur <= '9')) {
		ret *= 10;
		ret += *cur - '0';
		cur++;
	      }
	      if ((*cur != 0) && (*cur != ' ') && (*cur != '\t'))
		code = -1;
	      code = ret;
	    }
	  xmlFree (status);
	}
      else if (nd_dav_name_equal (cur, "responsedescription"))
	{
	}
      cur = cur->next;
    }
  return code;
}

ndNodeInfoPtr
nd_parse_prop_href(cur)
     xmlNodePtr cur;
{
  ndNodeInfoPtr ret;

  ret = ndNodeInfoNew();
  ret->name = (char *) xmlNodeGetContent(cur);
  return (ret);
}

ndNodeInfoPtr
nd_parse_response(cur)
     xmlNodePtr cur;
{
  ndNodeInfoPtr ret, last, res = NULL;

  ret = last = NULL;
  cur = cur->children;

  /*
   * response (href, ((href*, status)|(propstat+)), responsedescription?
   */
  while (cur != NULL)
    {
      if (nd_dav_name_equal (cur, "href"))
	{
	  res = nd_parse_prop_href (cur);
	  if (res != NULL)
	    {
	      if (ret == NULL) ret = last = res;
	      else last->next = res;
	      while (last->next != NULL) last = last->next;
	    }
	}
      else if (nd_dav_name_equal (cur, "propstat"))
	{
	  if (res)
	    {
	      res->status = nd_parse_propstat(cur, res);
	    }
	}
      else if (nd_dav_name_equal (cur, "status"))
	{
	  if (res)
	    {
	      char *status = (char *) xmlNodeGetContent (cur);
	      char *cur = status;
	      if (!strncmp(status, "HTTP/", 5))
		{
		  int version = 0;
		  int ret = 0;

		  cur += 5;
		  while ((*cur >= '0') && (*cur <= '9')) {
		    version *= 10;
		    version += *cur - '0';
		    cur++;
		  }
		  if (*cur == '.') {
		    cur++;
		    if ((*cur >= '0') && (*cur <= '9')) {
		      version *= 10;
		      version += *cur - '0';
		      cur++;
		    }
		    while ((*cur >= '0') && (*cur <= '9'))
		      cur++;
		  } else
		    version *= 10;
		  if ((*cur != ' ') && (*cur != '\t')) return NULL;
		  while ((*cur == ' ') || (*cur == '\t')) cur++;
		  if ((*cur < '0') || (*cur > '9')) return NULL;
		  while ((*cur >= '0') && (*cur <= '9')) {
		    ret *= 10;
		    ret += *cur - '0';
		    cur++;
		  }
		  if ((*cur != 0) && (*cur != ' ') && (*cur != '\t'))
		    return NULL;
		  res->status = ret;
		}
	      xmlFree (status);
	    }
	}
      else if (nd_dav_name_equal (cur, "responsedescription"))
	{
	}
      cur = cur->next;
    }
  return (ret);
}

ndNodeInfoPtr
nd_parse_multistatus (cur)
     xmlNodePtr cur;
{
  ndNodeInfoPtr ret = NULL, last, res;

  if (nd_dav_name_equal (cur, "multistatus"))
    {
      ret = last = NULL;
      cur = cur->children;
      while (cur != NULL)
	{
	  if (nd_dav_name_equal (cur, "response"))
	    {
	      res = nd_parse_response (cur);
	      if (res != NULL)
		{
		  if (ret == NULL) ret = last = res;
		  else last->next = res;
		  while (last->next != NULL) last = last->next;
		}
	    }
	  cur = cur->next;
	}
    }
  return ret;
}

int
ndPropFind (url, auth, prop, ns, depth, ni_return)
     char *url;
     ndAuthCtxtPtr auth;
     char *prop;
     char *ns;
     int depth;
     ndNodeInfoPtr *ni_return;
{
  ndNodeInfoPtr ret;
  xmlDocPtr doc;
  xmlBufferPtr buf = NULL;
  int code;

  if (prop == NULL)
    {
      code = nd_propfind_all_query (url, auth, depth, &buf);
    }
  else
    {
      code = nd_propfind_query (url, auth, prop, ns, depth, &buf);
    }
  if (code == -1)
    return -1;
  if (buf == NULL)
    return code;
  // fprintf (stderr, "%s\n", xmlBufferContent(buf));
  doc = xmlParseMemory(xmlBufferContent(buf), xmlBufferLength(buf));
  if (doc == NULL)
    {
      xmlBufferFree (buf);
      return -1;
    }
  ret = nd_parse_multistatus (doc->children);
  xmlFreeDoc(doc);
  xmlBufferFree(buf);
  *ni_return = ret;

  return code;
}

/*
 * PROPPATCH
 */
int
ndPropPatch (url, auth, prop, value, ns, lock_token, ni_return)
     char *url;
     ndAuthCtxtPtr auth;
     char *prop;
     char *value;
     char *ns;
     char *lock_token;
     ndNodeInfoPtr *ni_return;
{
  char proppatch_request [ND_REQUEST_MAX];
  char hstr [ND_HEADER_LINE_MAX];
  ndNodeInfoPtr ret;
  xmlBufferPtr buf = NULL;
  xmlDocPtr doc;
  int code;

  if (ns == NULL)
    ns = "DAV:";
  if (lock_token)
    {
      if (snprintf (hstr, ND_HEADER_LINE_MAX, "If: <%s> (<%s>)\r\n",
		    url, lock_token) >= ND_HEADER_LINE_MAX)
	{
	  return -1;
	}
    }
  else
    {
      hstr[0] = '\0';
    }
  if (value)
    {
      if (snprintf (proppatch_request, ND_REQUEST_MAX,
		    "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n\
<D:propertyupdate xmlns:D=\"DAV:\">\n\
<D:set>\n\
<D:prop><ns:%s xmlns:ns=\"%s\">%s</ns:%s></D:prop>\n\
</D:set>\n\
 </D:propertyupdate>", prop, ns, value, prop) >= ND_REQUEST_MAX)
	{
	  return -1;
	}
    }
  else
    { /* No value at all. REMOVE. */
      if (snprintf (proppatch_request, ND_REQUEST_MAX,
		    "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n\
<D:propertyupdate xmlns:D=\"DAV:\">\n\
<D:remove>\n\
<D:prop><ns:%s xmlns:ns=\"%s\"/></D:prop>\n\
</D:remove>\n\
 </D:propertyupdate>", prop, ns) >= ND_REQUEST_MAX)
	{
	  return -1;
	}
    }
  code = nd_dav_request ("PROPPATCH", url, auth, hstr, proppatch_request,
			 strlen (proppatch_request), &buf);
  if (code == -1)
    return -1;
  if (code == 207) /* Multi status. */
    {
      if (buf == NULL)
	return -1;
      doc = xmlParseMemory(xmlBufferContent(buf), xmlBufferLength(buf));
      if (doc == NULL)
	{
	  xmlBufferFree (buf);
	  return -1;
	}
      ret = nd_parse_multistatus (doc->children);
      xmlFreeDoc(doc);
      xmlBufferFree(buf);
      *ni_return = ret;
    }
  return code;
}

/*
 * PUT
 */
int
ndPut (url, auth, content, length, token, ni_return)
     char *url;
     ndAuthCtxtPtr auth;
     char *content;
     int   length;
     char *token;
     ndNodeInfoPtr *ni_return;
{
  void *ctxt;
  char if_header[ND_HEADER_LINE_MAX];
  int code;
  ndNodeInfoPtr ret;
  xmlChar s [1024];
  xmlBufferPtr buf;
  xmlDocPtr doc;
  int len;

  if (url == NULL) return -1;
  if_header [0] = '\0';
  if (token)
    {
      if (snprintf (if_header, ND_HEADER_LINE_MAX,
		    "If: <%s> (<%s>)\r\n", url, token) >= ND_HEADER_LINE_MAX)
	{
	  return -1;
	}
    }
  ctxt = ndHTTPMethod (url, auth, "PUT", content, NULL,
		       token? if_header : NULL,
		       length);
  if (ctxt == NULL) {
    return -1;
  }
  code = xmlNanoHTTPReturnCode(ctxt);
  if (code == 207) /* Multi status. */
    {
      buf = xmlBufferCreate ();
      if (buf == NULL) return -1;
      while ( (len = xmlNanoHTTPRead (ctxt, s, sizeof (s))) > 0)
	{
	  xmlBufferAdd (buf, s, len);
	}

      doc = xmlParseMemory(xmlBufferContent(buf), xmlBufferLength(buf));
      if (doc == NULL)
	{
	  xmlBufferFree (buf);
	  return -1;
	}
      ret = nd_parse_multistatus (doc->children);
      xmlFreeDoc(doc);
      xmlBufferFree (buf);
      *ni_return = ret;
    }
  xmlNanoHTTPClose (ctxt);
  return code;
}

/*
 * POST
 */

int
ndPost (url, auth, content, length, content_type, buf_return)
     char *url;
     ndAuthCtxtPtr auth;
     char *content;
     int length;
     char **content_type;
     xmlBufferPtr *buf_return;
{
  void *ctxt;
  int len;
  xmlChar buffer [1024];
  xmlBufferPtr output;
  int returnCode;

  if (url == NULL) return -1;
  ctxt = ndHTTPMethod(url, auth, "POST", content, content_type, NULL,
		      length);
  if (ctxt == NULL) {
    return -1;
  }
  returnCode = xmlNanoHTTPReturnCode(ctxt);
  if (returnCode >= 300)
    {
      xmlNanoHTTPClose(ctxt);
      if (content_type != NULL) xmlFree(content_type);
      return returnCode;
    }
  output = xmlBufferCreate ();
  if (output == NULL) return -1;
  while ((len = xmlNanoHTTPRead (ctxt, buffer, sizeof (buffer))) > 0)
    {
      xmlBufferAdd (output, buffer, len);
    }
  xmlNanoHTTPClose(ctxt);
  *buf_return = output;
  return returnCode;
}

int
ndPostPrint (url, auth, content, length, content_type, outfp)
     char *url;
     ndAuthCtxtPtr auth;
     char *content;
     int length;
     char **content_type;
     FILE *outfp;
{
  void *ctxt;
  int len;
  xmlChar buffer [32];
  int returnCode;

  if (url == NULL) return -1;

  ctxt = ndHTTPMethod(url, auth, "POST", content, content_type, NULL,
		      length);
  if (ctxt == NULL) return -1;

  returnCode = xmlNanoHTTPReturnCode(ctxt);
  if (returnCode >= 300)
    {
      xmlNanoHTTPClose(ctxt);
      if (content_type != NULL) xmlFree(content_type);
      return returnCode;
    }
  while ((len = xmlNanoHTTPRead (ctxt, buffer, sizeof (buffer))) > 0)
    {
      fwrite (buffer, len, sizeof (xmlChar), outfp);
    }
  xmlNanoHTTPClose(ctxt);
  return returnCode;
}

/*
 * DELETE
 */
int
ndDelete (url, auth, token)
     char *url;
     ndAuthCtxtPtr auth;
     char *token;
{
  void *ctxt;
  char if_header[ND_HEADER_LINE_MAX];
  int code;

  if (url == NULL) return -1;
  if_header [0] = '\0';
  if (token)
    {
      if (snprintf (if_header, ND_HEADER_LINE_MAX,
		    "If: <%s> (<%s>)\r\n", url, token) >= ND_HEADER_LINE_MAX)
	{
	  return -1;
	}
    }
  ctxt = ndHTTPMethod (url, auth, "DELETE", NULL, NULL,
		       token? if_header : NULL,
		       0);
  if (ctxt == NULL) {
    return(-1);
  }
  code = xmlNanoHTTPReturnCode(ctxt);
  xmlNanoHTTPClose (ctxt);
  return code;
}

ndLockInfoPtr
nd_parse_lock_answer (cur)
     xmlNodePtr cur;
{
  ndLockInfoPtr ret = NULL;

  if (nd_dav_name_equal (cur, "prop"))
    {
      ret = NULL;
      cur = cur->children;
      while (cur != NULL)
	{
	  if (nd_dav_name_equal (cur, "lockdiscovery"))
	    {
	      ret = nd_parse_lockdiscovery (cur);
	    }
	  cur = cur->next;
	}
    }
  return (ret);
}

int
nd_lock_request (url, auth, depth, owner, scope, timeout, buf_return)
     char *url;
     ndAuthCtxtPtr auth;
     int  depth;
     char *owner;
     int   scope;
     char *timeout;
     xmlBufferPtr *buf_return;
{
  char lock_request[ND_REQUEST_MAX];
  char scope_str[16];
  char hstr [ND_HEADER_LINE_MAX];
  char *depth_str;

  if (!owner)
    {
      owner = "unknown";
    }
  switch (scope)
    {
    case ND_LOCK_SCOPE_EXCLUSIVE:
      sprintf (scope_str, "exclusive");
      break;
    case ND_LOCK_SCOPE_SHARED:
      sprintf (scope_str, "shared");
      break;
    default:
      sprintf (scope_str, "none");
      break;
    }
  if (snprintf (lock_request, ND_REQUEST_MAX,
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
<D:lockinfo xmlns:D='DAV:'>\
<D:lockscope><D:%s/></D:lockscope>\
<D:locktype><D:write/></D:locktype>\
<D:owner>\
<D:href>%s</D:href>\
</D:owner>\
</D:lockinfo>", scope_str, owner) >= ND_REQUEST_MAX)
    {
      return -1;
    }
  switch (depth)
    {
    case ND_DEPTH_0:
    case ND_DEPTH_1: /* DEPTH 1 is not allowed. */
      depth_str = "0";
      break;
    case ND_DEPTH_INFINITE:
      depth_str = "Infinity";
      break;
    }
  if (timeout)
    {
      if (snprintf (hstr, ND_HEADER_LINE_MAX,
		    "Timeout: %s\r\nDepth: %s\r\n", timeout, depth_str)
	  >= ND_HEADER_LINE_MAX)
	{
	  return -1;
	}
    }
  else
    sprintf (hstr, "Depth: %s\r\n", depth_str);
  return nd_dav_request ("LOCK", url, auth, hstr, lock_request,
			 strlen (lock_request), buf_return);
}

int
ndLock (url, auth, depth, owner, scope, timeout, li_return)
     char *url;
     ndAuthCtxtPtr auth;
     int   depth;
     char *owner;
     int   scope;
     char *timeout;
     ndLockInfoPtr *li_return;
{
  ndLockInfoPtr ret;
  xmlDocPtr doc;
  xmlBufferPtr buf = NULL;
  int code;

  code = nd_lock_request (url, auth,
			  depth, owner, scope, timeout, &buf);
  if (buf != NULL)
    {
      doc = xmlParseMemory(xmlBufferContent(buf), xmlBufferLength(buf));
      if (doc == NULL)
	{
	  xmlBufferFree (buf);
	  return -1;
	}
      ret = nd_parse_lock_answer (doc->children);
      xmlFreeDoc(doc);
      xmlBufferFree (buf);
      *li_return = ret;
    }
  return code;

}

int
ndUnlock (url, auth, depth, token)
     char *url;
     ndAuthCtxtPtr auth;
     int  depth;
     char *token;
{
  void *ctxt;
  char hstr [ND_HEADER_LINE_MAX];
  int code;
  char *depth_str;

  switch (depth)
    {
    case ND_DEPTH_0:
      depth_str = "0";
      break;
    case ND_DEPTH_1:
      depth_str = "1";
      break;
    case ND_DEPTH_INFINITE:
      depth_str = "Infinity";
      break;
    }

  if (url == NULL || token == NULL) return -1;
  if (snprintf (hstr, ND_HEADER_LINE_MAX,
		"Lock-token: <%s>\r\nDepth: %s\r\n", token, depth_str)
      >= ND_HEADER_LINE_MAX)
    {
      return -1;
    }
  ctxt = ndHTTPMethod (url, auth, "UNLOCK", NULL, NULL,
		       hstr, 0);
  if (ctxt == NULL)
    {
      return -1;
    }
  code = xmlNanoHTTPReturnCode(ctxt);
  xmlNanoHTTPClose (ctxt);
  return code;
}

int
ndMkCol (url, auth, token)
     char *url;
     ndAuthCtxtPtr auth;
     char *token;
{
  void *ctxt;
  char if_header[ND_HEADER_LINE_MAX];
  int code;

  if (url == NULL) return -1;
  if_header [0] = '\0';
  if (token)
    {
      if (snprintf (if_header, ND_HEADER_LINE_MAX,
		    "If: <%s> (<%s>)\r\n", url, token) >= ND_HEADER_LINE_MAX)
	{
	  return -1;
	}
    }
  ctxt = ndHTTPMethod (url, auth, "MKCOL", NULL, NULL,
		       token? if_header : NULL, 0);
  if (ctxt == NULL)
    {
      return -1;
    }
  code = xmlNanoHTTPReturnCode(ctxt);
  xmlNanoHTTPClose (ctxt);
  return code;
}

int
ndGet (url, auth, ct_return, buf_return)
     char *url;
     ndAuthCtxtPtr auth;
     char **ct_return;
     xmlBufferPtr *buf_return;
{
  void *ctxt;
  char *contentType;
  int len;
  xmlChar buffer [1024];
  xmlBufferPtr output;
  int returnCode;

  if (url == NULL) return -1;

  ctxt = ndHTTPMethod(url, auth, "GET", NULL, &contentType, NULL, 0);
  if (ctxt == NULL) {
    return -1;
  }

  returnCode = xmlNanoHTTPReturnCode(ctxt);
  if (returnCode >= 300)
    {
      xmlNanoHTTPClose(ctxt);
      if (contentType != NULL) xmlFree(contentType);
      return returnCode;
    }
  *ct_return = contentType;
  output = xmlBufferCreate ();
  if (output == NULL) return -1;
  while ((len = xmlNanoHTTPRead (ctxt, buffer, sizeof (buffer))) > 0)
    {
      xmlBufferAdd (output, buffer, len);
    }
  xmlNanoHTTPClose(ctxt);
  *buf_return = output;
  return returnCode;
}


int
ndGetPrint (url, auth, ct_return, outfp)
     char *url;
     ndAuthCtxtPtr auth;
     char **ct_return;
     FILE *outfp;
{
  void *ctxt;
  char *contentType = NULL;
  int len;
  xmlChar buffer [32];
  int returnCode;

  if (url == NULL) return -1;

  ctxt = ndHTTPMethod(url, auth, "GET", NULL, &contentType, NULL, 0);
  if (ctxt == NULL) return -1;

  returnCode = xmlNanoHTTPReturnCode(ctxt);
  if (returnCode >= 300)
    {
      xmlNanoHTTPClose(ctxt);
      if (contentType != NULL) xmlFree(contentType);
      return returnCode;
    }
  *ct_return = contentType;

  while ((len = xmlNanoHTTPRead (ctxt, buffer, sizeof (buffer))) > 0)
    {
      fwrite (buffer, len, sizeof (xmlChar), outfp);
    }
  xmlNanoHTTPClose(ctxt);
  return returnCode;
}

int
ndMove (url, auth, dest_url, overwrite, lock_token)
     char *url;
     ndAuthCtxtPtr auth;
     char *dest_url;
     int overwrite;
     char *lock_token;
{
  void *ctxt;
  char hstr [ND_HEADER_LINE_MAX*3];
  char hstr_1 [ND_HEADER_LINE_MAX];
  int code;

  if (url == NULL || dest_url == NULL) return -1;
  if (snprintf (hstr, ND_HEADER_LINE_MAX*3,
		"Destination: %s\r\nOverwrite: %c\r\n", dest_url,
		overwrite?  'T' : 'F') >= ND_HEADER_LINE_MAX*3)
    {
      return -1;
    }
  if (lock_token)
    {
      if (snprintf (hstr_1, ND_HEADER_LINE_MAX,
		    "If: <%s> (<%s>)\r\n", dest_url, lock_token)
	  >= ND_HEADER_LINE_MAX)
	{
	  return -1;
	}
      if ((strlen (hstr) + strlen (hstr_1) + 1) > ND_HEADER_LINE_MAX * 3)
	{
	  return -1;
	}
      strcat (hstr, hstr_1);
    }
  ctxt = ndHTTPMethod (url, auth, "MOVE", NULL, NULL, hstr, 0);
  if (ctxt == NULL) {
    return(-1);
  }
  code = xmlNanoHTTPReturnCode(ctxt);
  xmlNanoHTTPClose (ctxt);
  return code;
}

int
ndCopy (url, auth, dest_url, overwrite, lock_token)
     char *url;
     ndAuthCtxtPtr auth;
     char *dest_url;
     int overwrite;
     char *lock_token;
{
  void *ctxt;
  char hstr [ND_HEADER_LINE_MAX*3];
  char hstr_1 [ND_HEADER_LINE_MAX];
  int code;

  if (url == NULL || dest_url == NULL) return -1;
  if (snprintf (hstr, ND_HEADER_LINE_MAX*3,
		"Destination: %s\r\nOverwrite: %c\r\n", dest_url,
		overwrite?  'T' : 'F') >= ND_HEADER_LINE_MAX*3)
    {
      return -1;
    }
  if (lock_token)
    {
      if (snprintf (hstr_1, ND_HEADER_LINE_MAX,
		    "If: <%s> (<%s>)\r\n", dest_url, lock_token)
	  >= ND_HEADER_LINE_MAX)
	{
	  return -1;
	}
      if ((strlen (hstr) + strlen (hstr_1) + 1) > ND_HEADER_LINE_MAX * 3)
	{
	  return -1;
	}
      strcat (hstr, hstr_1);
    }
  ctxt = ndHTTPMethod (url, auth, "COPY", NULL, NULL, hstr, 0);
  if (ctxt == NULL) {
    return(-1);
  }
  code = xmlNanoHTTPReturnCode(ctxt);
  xmlNanoHTTPClose (ctxt);
  return code;
}

char *
ndReasonPhrase (code)
     int code;
{
  switch (code)
    {
    case 100:
      return ND_CODE_100;
    case 101:
      return ND_CODE_101;
    case 102:
      return ND_CODE_102;
    case 200:
      return ND_CODE_200;
    case 201:
      return ND_CODE_201;
    case 202:
      return ND_CODE_202;
    case 203:
      return ND_CODE_203;
    case 204:
      return ND_CODE_204;
    case 205:
      return ND_CODE_205;
    case 206:
      return ND_CODE_206;
    case 207:
      return ND_CODE_207;
    case 300:
      return ND_CODE_300;
    case 301:
      return ND_CODE_301;
    case 302:
      return ND_CODE_302;
    case 303:
      return ND_CODE_303;
    case 304:
      return ND_CODE_304;
    case 305:
      return ND_CODE_305;
    case 307:
      return ND_CODE_307;
    case 400:
      return ND_CODE_400;
    case 401:
      return ND_CODE_401;
    case 402:
      return ND_CODE_402;
    case 403:
      return ND_CODE_403;
    case 404:
      return ND_CODE_404;
    case 405:
      return ND_CODE_405;
    case 406:
      return ND_CODE_406;
    case 407:
      return ND_CODE_407;
    case 408:
      return ND_CODE_408;
    case 409:
      return ND_CODE_409;
    case 410:
      return ND_CODE_410;
    case 411:
      return ND_CODE_411;
    case 412:
      return ND_CODE_412;
    case 413:
      return ND_CODE_413;
    case 414:
      return ND_CODE_414;
    case 415:
      return ND_CODE_415;
    case 416:
      return ND_CODE_416;
    case 417:
      return ND_CODE_417;
    case 422:
      return ND_CODE_422;
    case 423:
      return ND_CODE_423;
    case 424:
      return ND_CODE_424;
    case 500:
      return ND_CODE_500;
    case 501:
      return ND_CODE_501;
    case 502:
      return ND_CODE_502;
    case 503:
      return ND_CODE_503;
    case 504:
      return ND_CODE_504;
    case 505:
      return ND_CODE_505;
    case 507:
      return ND_CODE_507;
    }
  return ND_CODE_OTHER;
}
