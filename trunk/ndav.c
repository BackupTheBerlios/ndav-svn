/*
 * ndav.c - a small program for WebDAV operations.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>
#endif /* HAVE_SYS_CDEFS_H */

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <libxml/nanohttp.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>

#include "ndav.h"

#define NAMESPACE_TEMPL " xmlns:ns=\"%s\""

#define PROPFIND_HEAD \
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" \
		"<D:propfind xmlns:D=\"DAV:\">\n"

#define PROPFIND_TAIL "</D:propfind>\n"

#define PROPUPDATE_HEAD \
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" \
		"<D:propertyupdate xmlns:D=\"DAV:\">\n"

#define PROPUPDATE_TAIL "</D:propertyupdate>\n"

#define PROPSET_HEAD "\t<D:set>\n"

#define PROPSET_TAIL "\t</D:set>\n"

#define PROPREMOVE_HEAD "\t<D:remove>\n"

#define PROPREMOVE_TAIL "\t</D:remove>\n"

struct live_dav_prop {
	char *prop;
	int protected;
	void (*action)(xmlNodePtr, ndNodeInfoPtr);
};

const char *export_method = "";
const char *export_uri = "";

const char *depth_header[] = {
	"Depth: 0\r\n",
	"Depth: 1\r\n",
	"Depth: Infinity\r\n"
};

void ndav_get_creationdate(xmlNodePtr, ndNodeInfoPtr);
void ndav_get_resourcetype(xmlNodePtr, ndNodeInfoPtr);
void ndav_get_contentlanguage(xmlNodePtr, ndNodeInfoPtr);
void ndav_get_contentlength(xmlNodePtr, ndNodeInfoPtr);
void ndav_get_contenttype(xmlNodePtr, ndNodeInfoPtr);
void ndav_get_etag(xmlNodePtr, ndNodeInfoPtr);
void ndav_get_lastmodified(xmlNodePtr, ndNodeInfoPtr);
void ndav_get_lockdiscovery(xmlNodePtr, ndNodeInfoPtr);

ndLockInfoPtr nd_parse_lockdiscovery(xmlNodePtr);

struct live_dav_prop live_dav_props[] = {
		/* General properties. */
	{ "creationdate", 1, ndav_get_creationdate },
	{ "displayname", 0, /* not implemented */ NULL },
	{ "resourcetype", 1, ndav_get_resourcetype },
		/* For retreivable objects. */
	{ "getcontentlanguage", 0, ndav_get_contentlanguage },
	{ "getcontentlength", 1, ndav_get_contentlength },
	{ "getcontenttype", 1, ndav_get_contenttype },
	{ "getetag", 1, ndav_get_etag },
	{ "getlastmodified", 1, ndav_get_lastmodified },
		/* Locking properties. */
	{ "lockdiscovery", 1, ndav_get_lockdiscovery },
	{ "source", 1, /* not implemented, only RFC 2518 */ NULL },
	{ "supportedlock", 1, /* not implemented */ NULL },
		/* End marker. */
	{ NULL, 0, NULL }
}; /* live_dav_props */

int
is_live_dav_prop(const char *prop)
{
	struct live_dav_prop *ldp;

	for (ldp = live_dav_props; ldp->prop; ldp++)
		if ( !strcmp(ldp->prop, prop) && ldp->action )
			return 1;

	return 0;
}; /* is_live_dav_prop(const char *) */

ndNodeInfoPtr ndNodeInfoNew() {
	ndNodeInfoPtr ret = xmlMalloc(sizeof(ndNodeInfo));
	if (ret == NULL)
		return NULL;
	memset(ret, 0, sizeof(ndNodeInfo));
	return(ret);
}; /* ndNodeInfoNew() */

void ndNodeInfoFree(ndNodeInfoPtr info) {
	if (info == NULL)
		return;
	if (info->name)
		xmlFree(info->name);
	if (info->date)
		xmlFree(info->date);
	if (info->cdate)
		xmlFree(info->cdate);
	if (info->size)
		xmlFree(info->size);
	if (info->content)
		xmlFree(info->content);
	if (info->restype)
		xmlFree(info->restype);
	if (info->props)
		ndPropListFree(info->props);
	if (info->lock)
		ndLockInfoListFree(info->lock);
	xmlFree(info);
}

void ndNodeInfoListFree(ndNodeInfoPtr info) {
	ndNodeInfoPtr cur = info;

	while (cur != NULL) {
			info = info->next;
			ndNodeInfoFree(cur);
			cur = info;
	}
}; /* ndNodeInfoListFree(ndNodeInfoPtr) */

void ndNodeInfoPrint(FILE * fp, ndNodeInfoPtr info, int format) {
	if ( ! NDAV_PRINT_SEXP(format) ) {
		if (info == NULL)
			return;
		fprintf(fp, "Name: %s\n", info->name ? info->name : "");
		if ( NDAV_PRINT_VERBOSELY(format) ) {
			fprintf(fp, "Status: %d\n", info->status);
			fprintf(fp, "Last-Modified: %s\n", info->date ? info->date : "");
			fprintf(fp, "Created: %s\n", info->cdate ? info->cdate : "");
			fprintf(fp, "ETag: %s\n", info->etag ? info->etag : "");
			fprintf(fp, "Size: %s\n", info->size ? info->size : "");
			fprintf(fp, "Content-Type: %s\n",
					info->content ? info->content : "");
			fprintf(fp, "Lang: %s\n", info->lang ? info->lang : "");
			fprintf(fp, "Resource-Type: %s\n",
					info->restype ? info->restype : "");
		} else {
			if (info->status)
				fprintf(fp, "Status: %d\n", info->status);
			if (info->date)
				fprintf(fp, "Last-Modified: %s\n", info->date);
			if (info->cdate)
				fprintf(fp, "Created: %s\n", info->cdate);
			if (info->etag)
				fprintf(fp, "ETag: %s\n", info->etag);
			if (info->size)
				fprintf(fp, "Size: %s\n", info->size);
			if (info->content)
				fprintf(fp, "Content-Type: %s\n", info->content);
			if (info->lang)
				fprintf(fp, "Lang: %s\n", info->lang);
			if (info->restype)
				fprintf(fp, "Resource-Type: %s\n", info->restype);
		}

		if (info->props)
			ndPropListPrint(fp, info->props, format);
		if (info->lock)
			ndLockInfoListPrint(fp, info->lock, format);
		//fprintf(fp, "\n");
	}
	else { /* S-expression output. */
		if (info == NULL)
			return;
		fprintf(fp, "(\"%s\"", info->name ? info->name : "");
		if ( NDAV_PRINT_VERBOSELY(format))  {
			if (info->date)
				fprintf(fp, " (last-modified \"%s\")", info->date);
			if (info->cdate)
				fprintf(fp, " (created \"%s\")", info->cdate);
			if (info->status)
				fprintf(fp, " (status %d)", info->status);
			if (info->size)
				fprintf(fp, " (size %s)", info->size);
			if (info->content)
				fprintf(fp, " (content-type \"%s\")", info->content);
			if (info->restype)
				fprintf(fp, " (resourcetype \"%s\")", info->restype);
		}; /* Print verbosely. */
		if (info->props) {
			fprintf(fp, " ");
			ndPropListPrint(fp, info->props, format);
		}
		if (info->lock) {
			fprintf(fp, " ");
			ndLockInfoListPrint(fp, info->lock, format);
		}
		fprintf(fp, ")");
	}
}; /* ndNodeInfoPrint(FILE *, ndNodeInfoPtr, int) */

void ndNodeInfoListPrint(FILE * fp, ndNodeInfoPtr info, int format) {
	ndNodeInfoPtr cur = info;

	if ( NDAV_PRINT_SEXP(format) )
		fprintf(fp, "(");

	while (cur != NULL) {
		info = info->next;
		ndNodeInfoPrint(fp, cur, format);
		cur = info;
		if ( NDAV_PRINT_SEXP(format) && cur != NULL)
			fprintf(fp, " ");
	}
	if ( NDAV_PRINT_SEXP(format) )
		fprintf(fp, ")\n");
}/* ndNodeInfoListPrint(FILE *, ndNodeInfoPtr, int) */

ndLockInfoPtr ndLockInfoNew() {
	ndLockInfoPtr ret = xmlMalloc(sizeof(ndLockInfo));
	if (ret == NULL)
		return NULL;
	memset(ret, 0, sizeof(ndLockInfo));
	return ret;
}/* ndLockInfoNew() */

void ndLockInfoFree(ndLockInfoPtr lock) {
	if (lock == NULL)
		return;
	if (lock->scope)
		xmlFree(lock->scope);
	if (lock->type)
		xmlFree(lock->type);
	if (lock->owner_href)
		xmlFree(lock->owner_href);
	if (lock->token)
		xmlFree(lock->token);
	if (lock->timeout)
		xmlFree(lock->timeout);
	xmlFree(lock);
}; /* ndLockInfoFree(ndLockInfoPtr) */

void ndLockInfoListFree(ndLockInfoPtr info) {
	ndLockInfoPtr cur = info;

	while (cur != NULL) {
			info = info->next;
			ndLockInfoFree(cur);
			cur = info;
	}
}; /* ndLockInfoListFree(ndLockInfoPtr) */

void ndLockInfoPrint(FILE * fp, ndLockInfoPtr lock, int format) {
	if ( NDAV_PRINT_HEADER(format) ) {
		if (lock == NULL)
			return;
		if ( NDAV_PRINT_VERBOSELY(format) ) {
			fprintf(fp, "Lock: ");
			if (lock->token)
				fprintf(fp, "token=\"%s\",\n", lock->token);
			fprintf(fp, "      scope=\"%s\",\n",
					lock->scope ? lock->scope : "");
			fprintf(fp, "      owner-href=\"%s\",\n",
					 lock->owner_href ? lock->owner_href : "");
			fprintf(fp, "      timeout=\"%s\"\n",
					lock->timeout ? lock->timeout : "");
		} else {
			if (lock->token)
				fprintf(fp, "token=\"%s\"\n", lock->token);
		}
	}
	else if ( NDAV_PRINT_SEXP(format) ) {
		fprintf(fp, "(lock");
		if (lock->token)
			fprintf(fp, " (token \"%s\")", lock->token);
		fprintf(fp, " (scope %s)", lock->scope ? lock->scope : "");
		fprintf(fp, " (owner-href \"%s\")",
				 lock->owner_href ? lock->owner_href : "");
		fprintf(fp, " (timeout \"%s\"))",
				 lock->timeout ? lock->timeout : "");
	}
}; /* ndLockInfoPrint(FILE *, ndLockInfoPtr, int) */

void ndLockInfoListPrint(FILE * fp, ndLockInfoPtr info, int format) {
	ndLockInfoPtr cur = info;

	if ( NDAV_PRINT_SEXP(format) )
		fprintf(fp, "(lock-list ");
	while (cur != NULL) {
			info = info->next;
			ndLockInfoPrint(fp, cur, format);
			cur = info;
			if ( ( NDAV_PRINT_SEXP(format) ) && cur != NULL)
				fprintf(fp, " ");
	}
	if ( NDAV_PRINT_SEXP(format) )
		fprintf(fp, ")\n");
} /* ndLockInfoListPrint(FILE *, ndLockInfoPtr, int) */

ndPropPtr ndPropNew() {
	ndPropPtr ret = xmlMalloc(sizeof(ndProp));
	if (ret == NULL)
		return (NULL);
	memset(ret, 0, sizeof(ndProp));
	return ret;
}; /* ndPropNew() */

void ndPropFree(ndPropPtr prop)
{
	if (prop == NULL)
		return;
	if (prop->name)
		xmlFree(prop->name);
	if (prop->value)
		xmlFree(prop->value);
	if (prop->ns)
		xmlFree(prop->ns);
	xmlFree(prop);
}; /* ndPropFree(ndPropPtr) */

void ndPropListFree(ndPropPtr prop) {
	ndPropPtr cur = prop;

	while (cur != NULL) {
			prop = prop->next;
			ndPropFree(cur);
			cur = prop;
	}
}; /* ndPropListFree(ndPropPtr) */

void ndPropPrint(FILE * fp, ndPropPtr prop, int format) {
	if ( NDAV_PRINT_HEADER(format) ) {
		if (prop == NULL)
				return;
		if (prop->name) {
			if ( NDAV_PRINT_VERBOSELY(format) )
				fprintf(fp, "Property: ");
			if ( NDAV_PRINT_NAMEONLY(format) )
				fprintf(fp, "%s", prop->name);
			else if (prop->value && *prop->value == '\"')
				fprintf(fp, "%s=%s", prop->name, prop->value);
			else if (prop->value) {
				/* Remove initial whitespace. */
				char *val = prop->value + strspn(prop->value, " \t\n");

				fprintf(fp, "%s=\"%s\"", prop->name, val);
			}
		}
		if (prop->ns)
			fprintf(fp, "; ns=\"%s\"", prop->ns);
		fprintf(fp, "\n");
	} else if ( NDAV_PRINT_SEXP(format) ) {
		fprintf(fp, "(property");
		fprintf(fp, " (name \"%s\")", prop->name ? prop->name : "");
		if ( NDAV_PRINT_NAMEONLY(format) )
			;
		else if (prop->value && *prop->value == '\"')
			fprintf(fp, " (value %s)", prop->value);
		else
			fprintf(fp, " (value \"%s\")",
						prop->value ? prop->value : "");
		if (prop->ns)
			fprintf(fp, " (ns \"%s\")", prop->ns);
		fprintf(fp, ")");
	}
}; /* ndPropPrint(FILE *, ndPropPtr, int) */

void ndPropListPrint(FILE * fp, ndPropPtr prop, int format) {
	ndPropPtr cur = prop;

	while (cur != NULL) {
		prop = prop->next;
		ndPropPrint(fp, cur, format);
		cur = prop;
		if ( ( NDAV_PRINT_SEXP(format) ) && cur != NULL)
			fprintf(fp, " ");
	}
}; /* ndPropListPrint(FILE *, ndPropPtr, int) */

void * ndHTTPMethod( const char * URL,
					 ndAuthCtxtPtr auth,
					 const char * method,
					 const char * input,
					 char **contentType,
					 const char * headers,
					 int ilen)
{
	int returnCode;
	void * ctxt;
	char * auth_header = NULL;
	char * proxy_auth_header = NULL;
	char line[ND_HEADER_LINE_MAX];
	xmlBufferPtr header_buf = NULL;
	xmlBufferPtr temp_buf = NULL;
	char * uri = (char *) URL;

	if (URL == NULL)
		return NULL;

	if ( !(uri = strchr(uri, '/')) || !(uri = strchr(++uri, '/')) || ! *uri)
		return NULL;

	if ( !(uri = strchr(++uri, '/')))
		return NULL;

	export_method = method;
	export_uri = uri;
 
	header_buf = xmlBufferCreate();
	if (header_buf == NULL)
		return NULL;

	if (auth && (auth->pauth_realm)) {
		if ( snprintf(line, sizeof(line), "%s realm=\"%s\"",
					  auth->pauth_type, auth->pauth_realm)
				>= (int) sizeof(line) )
			return NULL;
			
		if ( ndAuthCreateHeader(line, auth->auth_cb, &temp_buf, 1) < 0 )
			return NULL;
		if (temp_buf == NULL)
			return NULL;

		xmlBufferAdd(header_buf, xmlBufferContent(temp_buf), -1);
		xmlBufferFree(temp_buf);
	}

	if (auth && (auth->auth_realm)) {
		if ( snprintf(line, sizeof(line), "%s realm=\"%s\"",
					  auth->auth_type, auth->auth_realm)
				>= (int) sizeof(line) )
			return NULL;

		if ( ndAuthCreateHeader(line, auth->auth_cb, &temp_buf, 0) < 0 )
			return NULL;
		if (temp_buf == NULL)
			return NULL;
			
		xmlBufferAdd(header_buf, xmlBufferContent(temp_buf), -1);
		xmlBufferFree(temp_buf);
	}

	if (headers)
		xmlBufferAdd(header_buf, (xmlChar *) headers, -1);

	ctxt = xmlNanoHTTPMethod(URL, method, input, contentType,
					xmlBufferLength(header_buf) == 0
								? NULL
								: (char *) xmlBufferContent(header_buf),
					ilen);
	xmlBufferFree(header_buf);

	if (ctxt == NULL)
		return NULL;

	returnCode = xmlNanoHTTPReturnCode(ctxt);
	while (returnCode == 401 || returnCode == 407) {
		auth_header = (char *) xmlNanoHTTPAuthHeader(ctxt);

		if (!auth || auth->auth_cb == NULL)
			return ctxt;

		if ( ndAuthCreateHeader(auth_header, auth->auth_cb,
					&header_buf, (returnCode == 407))
				< 0)
			return NULL;
			
		if (header_buf == NULL)
			return NULL;
		
		if (returnCode == 407) {
			if (proxy_auth_header)
				xmlFree(proxy_auth_header);
			proxy_auth_header =
				xmlMemStrdup((char *) xmlBufferContent(header_buf));
			xmlBufferFree(header_buf);
			header_buf = xmlBufferCreate();
		}
	
		if (headers)
			xmlBufferAdd(header_buf, (xmlChar *) headers, -1);
		if (proxy_auth_header)
			xmlBufferAdd(header_buf, (xmlChar *) proxy_auth_header, -1);

		/* XXX If HTTP/1.1 and Keep-Alive, no need to close here. */
		xmlNanoHTTPClose(ctxt);

		ctxt = xmlNanoHTTPMethod(URL, method, input, contentType,
							(char *) xmlBufferContent(header_buf), ilen);
		xmlBufferFree(header_buf);
		if (ctxt == NULL)
			return NULL;
		returnCode = xmlNanoHTTPReturnCode(ctxt);
	}
	
	if (proxy_auth_header)
		xmlFree(proxy_auth_header);
	if (auth && auth->notify_cb)
		auth->notify_cb(ctxt);
	
	return ctxt;
}; /* ndHTTPMethod(...) */

/* Generic DAV request function. */
int nd_dav_request( char * method,
					char * url,
					ndAuthCtxtPtr auth,
					const char * header,
					const char * content,
					int length,
					xmlBufferPtr * buf_return)
{
	int len, returnCode;
	void * ctxt;
	char * contentType = "text/xml; charset=\"utf-8\"";
	xmlChar buffer[1024];
	xmlBufferPtr output;

	if (url == NULL)
		return -1;

	ctxt = ndHTTPMethod(url, auth, method, content, &contentType, header,
					content ? strlen(content) : 0);
	if (ctxt == NULL)
			return -1;

	returnCode = xmlNanoHTTPReturnCode(ctxt);
	if ( (returnCode >= 300)
			|| ( (contentType != NULL)
					&& (strncmp(contentType, "text/xml", 8)) ) ) {
		xmlNanoHTTPClose(ctxt);
		if (contentType != NULL)
			xmlFree(contentType);
		return returnCode;
	}

	if (contentType != NULL)
		xmlFree(contentType);

	output = xmlBufferCreate();
	if (output == NULL)
		return -1;

	while ( (len = xmlNanoHTTPRead(ctxt, buffer,sizeof(buffer))) > 0 )
		xmlBufferAdd(output, buffer, len);

	xmlNanoHTTPClose(ctxt);
	*buf_return = output;

	return returnCode;
}; /* nd_dav_request(...) */

void ndav_get_creationdate(xmlNodePtr cur, ndNodeInfoPtr node) {
	node->cdate = (char *) xmlNodeGetContent(cur);
}; /* ndav_get_creationdate(xmlNodePtr, ndNodeInfoPtr) */

void ndav_get_resourcetype(xmlNodePtr cur, ndNodeInfoPtr node) {
	if (cur->children != NULL)
		node->restype = xmlMemStrdup((char *) cur->children->name);
}; /* ndav_get_resourcetype(xmlNodePtr, ndNodeInfoPtr) */

void ndav_get_contentlanguage(xmlNodePtr cur, ndNodeInfoPtr node) {
	node->lang = (char *) xmlNodeGetContent(cur);
}; /* ndav_get_contentlanguage(xmlNodePtr, ndNodeInfoPtr) */

void ndav_get_contentlength(xmlNodePtr cur, ndNodeInfoPtr node) {
	node->size = (char *) xmlNodeGetContent(cur);
}; /* ndav_get_contentlength(xmlNodePtr, ndNodeInfoPtr) */

void ndav_get_contenttype(xmlNodePtr cur, ndNodeInfoPtr node) {
	node->content = (char *) xmlNodeGetContent(cur);
}; /* ndav_get_contenttype(xmlNodePtr, ndNodeInfoPtr) */

void ndav_get_etag(xmlNodePtr cur, ndNodeInfoPtr node) {
	node->etag = (char *) xmlNodeGetContent(cur);
}; /* ndav_get_etag(xmlNodePtr, ndNodeInfoPtr) */

void ndav_get_lastmodified(xmlNodePtr cur, ndNodeInfoPtr node) {
	node->date = (char *) xmlNodeGetContent(cur);
}; /* ndav_get_lastmodified(xmlNodePtr, ndNodeInfoPtr) */

void ndav_get_lockdiscovery(xmlNodePtr cur, ndNodeInfoPtr node) {
	node->lock = nd_parse_lockdiscovery(cur);
}; /* ndav_get_lockdiscovery(xmlNodePtr, ndNodeInfoPtr) */

/*
 * PROPFIND(PROPNAME)
 */
int nd_propfind_propname_query(char * url, ndAuthCtxtPtr auth,
						 int depth, xmlBufferPtr * buf_return)
{
	const char * verboseQuery =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
		"<D:propfind xmlns:D=\"DAV:\"><D:propname/></D:propfind>\n";

	if (depth < ND_DEPTH_0 || depth > ND_DEPTH_INFINITE)
		depth = ND_DEPTH_0;

	return nd_dav_request("PROPFIND", url, auth,
						 depth_header[depth], verboseQuery,
						 strlen(verboseQuery), buf_return);
}; /* nd_propfind_propname_query(char *, ndAuthCtxtPtr, int, xmlBufferPtr *) */

/*
 * PROPFIND(ALLPROP)
 */
int nd_propfind_all_query(char * url, ndAuthCtxtPtr auth,
						 int depth, xmlBufferPtr * buf_return)
{
	const char * verboseQuery =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
		"<D:propfind xmlns:D=\"DAV:\"><D:allprop/></D:propfind>\n";

	if (depth < ND_DEPTH_0 || depth > ND_DEPTH_INFINITE)
		depth = ND_DEPTH_0;

	return nd_dav_request("PROPFIND", url, auth,
						 depth_header[depth], verboseQuery,
						 strlen(verboseQuery), buf_return);
}; /* nd_propfind_all_query(char *, ndAuthCtxtPtr, int, xmlBufferPtr *) */

/*
 * PROPFIND
 */
int
nd_propfind_query(char * url, ndAuthCtxtPtr auth, ndPropPtr req,
					 int depth, xmlBufferPtr * buf_return)
{
	int code;
	int live_prop = 0;
	size_t length, len;
	char *propfind_request, *prop_part;
	char *namespace = NULL;
	ndPropPtr prp;

	const char *request_templ =	"\t<D:prop%s><%s%s%s/></D:prop>\n";

	if (depth < ND_DEPTH_0 || depth > ND_DEPTH_INFINITE)
		depth = ND_DEPTH_0;

	length = sizeof(PROPFIND_HEAD PROPFIND_TAIL);

	propfind_request = (char *) malloc(length);
	if (!propfind_request)
		return -1;

	strncpy(propfind_request, PROPFIND_HEAD, length);

	for (prp = req; prp; prp = prp->next) {
		if (prp->type != NDPROP_FIND)
			continue;

		if (prp->ns && *prp->ns) {
			len = sizeof(NAMESPACE_TEMPL) + strlen(prp->ns);

			namespace = malloc(len);
			if (!namespace)
				return -1;

			snprintf(namespace, len, NAMESPACE_TEMPL, prp->ns);
		} else {
			live_prop = is_live_dav_prop(prp->name);
			namespace = NULL;
		}

		len = strlen(request_templ) +
				(namespace && *namespace ? strlen(namespace) : 0) +
				strlen("ns:") + strlen(prp->name) + 1;

		prop_part = (char *) malloc(len);
		if (!prop_part) {
			free(namespace);
			free(propfind_request);
			return -1;
		}

		if ( snprintf(prop_part, len, request_templ,
					  "",
					  namespace && *namespace ? "ns:"
											  : live_prop ? "D:" : "",
					  prp->name,
					  namespace && *namespace ? namespace : "")
			  >= (int) len) {
			free(namespace);
			free(propfind_request);
			free(prop_part);
			return -1;
		}

		free(namespace);	/* No more use.  */

		length += len;		/* Property selector usage.  */

		propfind_request = (char *) realloc(propfind_request, length);
		if (!propfind_request) {
			free(prop_part);
			return -1;
		}

		strncat(propfind_request, prop_part, len);
	}

	strncat(propfind_request, PROPFIND_TAIL,
			length - strlen(propfind_request));

	free(prop_part);	/* No more use.  */

	code = nd_dav_request("PROPFIND", url, auth,
							 depth_header[depth], propfind_request,
							 strlen(propfind_request), buf_return);
	free(propfind_request);

	return code;
}; /* nd_propfind_query(...) */

int nd_dav_name_equal(xmlNodePtr node, char * name) {
	if ( node->ns && node->ns->href
			&& (! strcmp((char *) node->ns->href, "DAV:")) )
		return ! strcmp((char *) node->name, name);
	else
		return 0;
}; /* nd_dav_name_equal(xmlNodePtr, char *) */

ndLockInfoPtr nd_parse_activelock(xmlNodePtr cur) {
	ndLockInfoPtr ret = NULL;

	ret = ndLockInfoNew();
	cur = cur->children;

	/*
	 * activelock(lockscpoe, locktype, depth, owner?, timeout? locktoken?)
	 */
	while (cur != NULL) {
		if ( nd_dav_name_equal(cur, "lockscope") ) {
			if (cur->children != NULL)
				ret->scope = xmlMemStrdup((char *) cur->children->name);
		}
		else if ( nd_dav_name_equal(cur, "owner") ) {
			if (cur->children != NULL)
				if (nd_dav_name_equal(cur->children, "href"))
					ret->owner_href =
						(char *) xmlNodeGetContent(cur->children);
		}
		else if (nd_dav_name_equal(cur, "locktype"))
			ret->type = (char *) xmlNodeGetContent(cur);
		else if (nd_dav_name_equal(cur, "locktoken")) {
			char * curp, * start;
			char * tmp = (char *) xmlNodeGetContent(cur);
			
			if (tmp) {
				curp = tmp;
				while (isspace(*curp))
					curp++;

				start = curp;
				while (! isspace(*curp))
					curp++;
				if (*curp != '\0')
					*curp = '\0';
				
				ret->token = xmlMemStrdup(start);
				xmlFree(tmp);
			}
		}
		else if (nd_dav_name_equal(cur, "depth"))
			ret->depth = (char *) xmlNodeGetContent(cur);

		else if (nd_dav_name_equal(cur, "timeout"))
			ret->timeout = (char *) xmlNodeGetContent(cur);
				
		cur = cur->next;
	}; /* while */

	return(ret);
}; /* nd_parse_activelock(xmlNodePtr) */

ndLockInfoPtr nd_parse_lockdiscovery(xmlNodePtr cur) {
	ndLockInfoPtr ret, last, res;

	ret = last = NULL;
	cur = cur->children;
	while (cur != NULL) {
		if (nd_dav_name_equal(cur, "activelock")) {
			res = nd_parse_activelock(cur);
			if (res != NULL) {
				if (ret == NULL)
					ret = last = res;
				else
					last->next = res;
				while (last->next != NULL)
					last = last->next;
			}
		}
		cur = cur->next;
	} /* while */

	return ret;
}; /* nd_parse_lockdiscovery(xmlNodePtr) */

void nd_parse_prop(xmlNodePtr cur, ndNodeInfoPtr node) {
	cur = cur->children;

	/*
	 * prop ANY
	 */
	while (cur != NULL) {
		struct live_dav_prop *ldp;
		int found = 0;

		for (ldp = live_dav_props; ldp->prop; ldp++)
			if ( nd_dav_name_equal(cur, ldp->prop) && ldp->action ) {
				found = 1;
				ldp->action(cur, node);
				break;
			}
		if ( !found && strcmp((char *) cur->name, "text") ) {
			/* XXX is this true? */
			ndPropPtr prop;
			prop = node->props;
			node->props = ndPropNew();
			node->props->name = xmlMemStrdup((char *) cur->name);
			node->props->value = (char *) xmlNodeGetContent(cur);
			if ( cur->ns && cur->ns->href
					&& strcmp((char *) cur->ns->href, "DAV:") )
				node->props->ns = xmlMemStrdup((char *) cur->ns->href);
			// ndPropPrint(stderr, node->props, format | ND_PRINT_AS_SEXP);
			node->props->next = prop;
		}
		cur = cur->next;
	}
}; /* nd_parse_prop(xmlNodePtr, ndNodeInfoPtr) */

int nd_parse_propstat(xmlNodePtr cur, ndNodeInfoPtr node) {
	int code = -1;
	cur = cur->children;

	/*
	 * propstat(prop, status, responsedescription?)
	 */
	while (cur != NULL) {
		if ( nd_dav_name_equal(cur, "prop") )
			nd_parse_prop(cur, node);
		else if ( nd_dav_name_equal(cur, "status") ) {
			/* Property status */
			char * status = (char *) xmlNodeGetContent(cur);
			char * cur = status;

			if (! strncmp(status, "HTTP/", 5)) {
				int version = 0;
				int ret = 0;

				cur += 5;
				while ( isdigit(*cur) ) {
					version *= 10;
					version += *cur - '0';
					cur++;
				}
				if (*cur == '.') {
					cur++;
					if ( isdigit(*cur) ) {
						version *= 10;
						version += *cur - '0';
						cur++;
					}
					while ( isdigit(*cur) )
						cur++;
				} else
					version *= 10;
			
				if ( (*cur != ' ') && (*cur != '\t') )
					return -1;

				while ( (*cur == ' ') || (*cur == '\t') )
					cur++;

				if ( ! isdigit(*cur) )
					return -1;
				 
				while ( isdigit(*cur) ) {
					ret *= 10;
					ret += *cur - '0';
					cur++;
				}

				if ( (*cur != 0) && (*cur != ' ') && (*cur != '\t') )
					code = -1;

				code = ret;
			}
			xmlFree(status);
		} /* case status */
		else if ( nd_dav_name_equal(cur, "responsedescription") )
		{
		} /* case responsedescription */
		cur = cur->next;
	} /* while */

	return code;
}; /* nd_parse_propstat(xmlNodePtr, ndNodeInfoPtr) */

ndNodeInfoPtr nd_parse_prop_href(xmlNodePtr cur) {
	ndNodeInfoPtr ret;

	ret = ndNodeInfoNew();
	ret->name = (char *) xmlNodeGetContent(cur);

	return ret;
}; /* nd_parse_prop_href(xmlNodePtr cur) */

ndNodeInfoPtr nd_parse_response(xmlNodePtr cur) {
	ndNodeInfoPtr ret, last, res = NULL;

	ret = last = NULL;
	cur = cur->children;

	/*
	 * response(href, ((href*, status)|(propstat+)), responsedescription?
	 */
	while (cur != NULL) {
		if ( nd_dav_name_equal(cur, "href") ) {
			res = nd_parse_prop_href(cur);
			if (res != NULL) {
				if (ret == NULL)
					ret = last = res;
				else
					last->next = res;
				while ( last->next != NULL )
					last = last->next;
			}
		} /* case href */
		else if ( nd_dav_name_equal(cur, "propstat") ) {
			if (res)
				res->status = nd_parse_propstat(cur, res);
		} /* case propstat */
		else if ( nd_dav_name_equal(cur, "status") ) {
			if (res) {
				char * status = (char *) xmlNodeGetContent(cur);
				char * cur = status;

				if (! strncmp(status, "HTTP/", 5)) {
					int version = 0;
					int ret = 0;

					cur += 5;
					while ( isdigit(*cur) ) {
						version *= 10;
						version += *cur - '0';
						cur++;
					}

					if (*cur == '.') {
						cur++;
						if ( isdigit(*cur) ) {
							version *= 10;
							version += *cur - '0';
							cur++;
						}
						while ( isdigit(*cur) )
							cur++;
					} else
						version *= 10;

					if ( (*cur != ' ') && (*cur != '\t') )
						return NULL;

					while ( (*cur == ' ') || (*cur == '\t') )
						cur++;

					if ( ! isdigit(*cur) )
						return NULL;

					while ( isdigit(*cur) ) {
						ret *= 10;
						ret += *cur - '0';
						cur++;
					}

					if ( (*cur != 0) && (*cur != ' ') && (*cur != '\t') )
						return NULL;

					res->status = ret;
				}
				xmlFree(status);
			}
		} /* case status */
		else if ( nd_dav_name_equal(cur, "responsedescription") )
		{
		} /* case responsedescription */

		cur = cur->next;
	} /* while */

	return ret;
}; /* nd_parse_response(xmlNodePtr cur) */

ndNodeInfoPtr nd_parse_multistatus(xmlNodePtr cur) {
	ndNodeInfoPtr ret = NULL, last, res;

	if ( nd_dav_name_equal(cur, "multistatus") ) {
		ret = last = NULL;
		cur = cur->children;
		while (cur != NULL) {
			if ( nd_dav_name_equal(cur, "response") ) {
				res = nd_parse_response(cur);
				if (res != NULL) {
					if (ret == NULL)
						ret = last = res;
					else
						last->next = res;

					while (last->next != NULL)
						last = last->next;
				}
			} /* case response */

			cur = cur->next;
		} /* while */
	} /* case multistatus */
	
	return ret;
}; /* nd_parse_multistatus(xmlNodePtr cur) */

int ndPropFind( char * url, ndAuthCtxtPtr auth, ndPropPtr req,
				int depth, int detect, ndNodeInfoPtr * ni_return)
{
	int code;
	ndNodeInfoPtr ret;
	xmlDocPtr doc;
	xmlBufferPtr buf = NULL;

	if (detect)
		code = nd_propfind_propname_query(url, auth, depth, &buf);
	else if (req == NULL || req->name == NULL)
		code = nd_propfind_all_query(url, auth, depth, &buf);
	else
		code = nd_propfind_query(url, auth, req, depth, &buf);

	if (code == -1)
		return -1;
	if (buf == NULL)
		return code;
	
	// fprintf(stderr, "%s\n", xmlBufferContent(buf));
	doc = xmlParseMemory((char *) xmlBufferContent(buf),
							xmlBufferLength(buf));
	if (doc == NULL) {
			xmlBufferFree(buf);
			return -1;
	}

	ret = nd_parse_multistatus(doc->children);
	xmlFreeDoc(doc);
	xmlBufferFree(buf);
	*ni_return = ret;

	return code;
}; /* propfind(...) */

/*
 * PROPPATCH
 */
int ndPropPatch(char * url, ndAuthCtxtPtr auth,
				ndPropPtr req, char * lock_token,
				ndNodeInfoPtr * ni_return)
{
	int code, done_set = 0, done_remove = 0;
	int live_prop = 0;
	size_t length, len;
	char *namespace = NULL;
	char *proppatch_request, *prop_part;
	char hstr[ND_HEADER_LINE_MAX];
	ndNodeInfoPtr ret;
	ndPropPtr prp;
	xmlBufferPtr buf = NULL;
	xmlDocPtr doc;

	const char update_string[] =
		"\t\t<D:prop><%s%s%s>%s</%s%s></D:prop>\n";
	const char update_remove[] =
		"\t\t<D:prop><%s%s%s/></D:prop>\n";

	if (lock_token) {
		if ( snprintf(hstr, sizeof(hstr),
					"If: <%s> (<%s>)\r\n", url, lock_token)
				>= (int) sizeof(hstr) ) {
			free(namespace);
			return -1;
		}
	}
	else
		hstr[0] = '\0';

	length = sizeof(PROPUPDATE_HEAD PROPUPDATE_TAIL)
				+ sizeof(PROPSET_HEAD PROPSET_TAIL)
				+ sizeof(PROPREMOVE_HEAD PROPREMOVE_TAIL);

	proppatch_request = (char *) malloc(length);
	if (!proppatch_request)
		return -1;

	strncpy(proppatch_request, PROPUPDATE_HEAD, length);

	for (prp = req; prp; prp = prp->next) {
		if (prp->type != NDPROP_PATCH)
			continue;

		if (!done_set) {
			strcat(proppatch_request, PROPSET_HEAD);
			++done_set;
		}

		if (prp->ns && *prp->ns) {
			len = sizeof(NAMESPACE_TEMPL) + strlen(prp->ns);

			namespace = malloc(len);
			if (!namespace)
				return -1;

			snprintf(namespace, len, NAMESPACE_TEMPL, prp->ns);
		} else {
			live_prop = is_live_dav_prop(prp->name);
			namespace = NULL;
		}

		len = strlen(update_string)
				+ 2 * strlen(prp->name)
				+ strlen(prp->value)
				+ (namespace && *namespace ? strlen(namespace) : 0)
				+ 2 * strlen("ns:");

		prop_part = (char *) malloc(len);
		if (!prop_part) {
			free(namespace);
			free(proppatch_request);
			return -1;
		}

		if ( snprintf(prop_part, len, update_string,
					  namespace && *namespace ? "ns:"
											  : live_prop ? "D:" : "",
					  prp->name,
					  namespace && *namespace ? namespace : "",
					  prp->value,
					  namespace && *namespace ? "ns:"
											  : live_prop ? "D:" : "",
					  prp->name) >= (int) len ) {
			free(namespace);
			free(proppatch_request);
			free(prop_part);
			return -1;
		}

		free(namespace);
		length += len;		/* Property specific addition.  */

		proppatch_request = (char *) realloc(proppatch_request, length);
		if (!proppatch_request) {
			free(prop_part);
			return -1;
		}

		strncat(proppatch_request, prop_part, len);
		free(prop_part);
	} /* for (prp->type == NDPROP_PATCH) */

	if (done_set)
		strcat(proppatch_request, PROPSET_TAIL);

	for (prp = req; prp; prp = prp->next) {
		if (prp->type != NDPROP_REMOVE)
			continue;

		if (!done_remove) {
			strcat(proppatch_request, PROPREMOVE_HEAD);
			++done_remove;
		}

		if (prp->ns && *prp->ns) {
			len = sizeof(NAMESPACE_TEMPL) + strlen(prp->ns);

			namespace = malloc(len);
			if (!namespace)
				return -1;

			snprintf(namespace, len, NAMESPACE_TEMPL, prp->ns);
		} else {
			live_prop = is_live_dav_prop(prp->name);
			namespace = NULL;
		}

		len = strlen(update_remove) + strlen(prp->name)
				+ (namespace && *namespace ? strlen(namespace) : 0)
				+ strlen("ns:");

		prop_part = (char *) malloc(len);
		if (!prop_part) {
			free(namespace);
			free(proppatch_request);
			return -1;
		}

		if ( snprintf(prop_part, len, update_remove,
					  namespace && *namespace ? "ns:"
											  : live_prop ? "D:" : "",
					  prp->name,
					  namespace && *namespace ? namespace : "")
			 >= (int) len ) {
			free(namespace);
			free(proppatch_request);
			free(prop_part);
			return -1;
		}

		free(namespace);
		length += len;		/* Property specific addition.  */

		proppatch_request = (char *) realloc(proppatch_request, length);
		if (!proppatch_request) {
			free(prop_part);
			return -1;
		}

		strncat(proppatch_request, prop_part, len);
		free(prop_part);
	} /* for (prp->type == NDPROP_REMOVE) */

	if (done_remove)
		strcat(proppatch_request, PROPREMOVE_TAIL);

	strncat(proppatch_request, PROPUPDATE_TAIL,
			length - strlen(proppatch_request));

	code = nd_dav_request("PROPPATCH", url, auth, hstr, proppatch_request,
						strlen(proppatch_request), &buf);

	free(proppatch_request);

	if (code == -1)
		return -1;

	if (code == 207) {
		/* Multi status. */
		if (buf == NULL)
			return -1;
		doc = xmlParseMemory((char *) xmlBufferContent(buf),
								xmlBufferLength(buf));
		if (doc == NULL) {
			xmlBufferFree(buf);
			return -1;
		}

		ret = nd_parse_multistatus(doc->children);
		xmlFreeDoc(doc);
		xmlBufferFree(buf);
		*ni_return = ret;
	} /* code 207 */

	return code;
}; /* ndPropPatch(...) */

/*
 * PUT
 */
int ndPut(char * url, ndAuthCtxtPtr auth, char * content, int length,
			char * token, ndNodeInfoPtr * ni_return)
{
	int code, len;
	void * ctxt;
	char if_header[ND_HEADER_LINE_MAX];
	ndNodeInfoPtr ret;
	xmlChar s[1024];
	xmlBufferPtr buf;
	xmlDocPtr doc;

	if (url == NULL)
		return -1;

	if_header[0] = '\0';
	if (token)
		if ( snprintf(if_header, sizeof(if_header),
						"If: <%s> (<%s>)\r\n", url, token)
				>= (int) sizeof(if_header) )
			return -1;

	ctxt = ndHTTPMethod(url, auth, "PUT", content, NULL,
							token ? if_header : NULL, length);
	if (ctxt == NULL)
		return -1;

	code = xmlNanoHTTPReturnCode(ctxt);
	if (code == 207) {
		/* Multi status. */
		buf = xmlBufferCreate();
		if (buf == NULL)
			return -1;
		while ( (len = xmlNanoHTTPRead(ctxt, s, sizeof(s))) > 0 )
			xmlBufferAdd(buf, s, len);

		doc = xmlParseMemory((char *) xmlBufferContent(buf),
								xmlBufferLength(buf));

		if (doc == NULL) {
			xmlBufferFree(buf);
			return -1;
		}

		ret = nd_parse_multistatus(doc->children);
		xmlFreeDoc(doc);
		xmlBufferFree(buf);
		*ni_return = ret;
	} /* code 207 */

	xmlNanoHTTPClose(ctxt);

	return code;
}; /* ndPut(...) */

/*
 * POST
 */

int ndPost(char * url, ndAuthCtxtPtr auth, char * content, int length,
		 char **content_type, xmlBufferPtr * buf_return)
{
	int len, returnCode;
	void * ctxt;
	xmlChar buffer[1024];
	xmlBufferPtr output;

	if (url == NULL)
		return -1;

	ctxt = ndHTTPMethod(url, auth, "POST", content, content_type,
							NULL, length);
	if (ctxt == NULL)
		return -1;

	returnCode = xmlNanoHTTPReturnCode(ctxt);
	if (returnCode >= 300) {
		xmlNanoHTTPClose(ctxt);

		if (content_type != NULL)
			xmlFree(content_type);
		
		return returnCode;
	} /* code >= 300 */

	output = xmlBufferCreate();
	if (output == NULL)
		return -1;

	while ( (len = xmlNanoHTTPRead(ctxt, buffer, sizeof(buffer))) > 0 )
		xmlBufferAdd(output, buffer, len);

	xmlNanoHTTPClose(ctxt);
	*buf_return = output;

	return returnCode;
}; /* ndPost(...) */

int ndPostPrint(char * url, ndAuthCtxtPtr auth, char * content,
				int length, char **content_type, FILE * outfp)
{
	int len, returnCode;
	void * ctxt;
	xmlChar buffer[32];

	if (url == NULL)
		return -1;

	ctxt = ndHTTPMethod(url, auth, "POST", content,
							content_type, NULL, length);
	if (ctxt == NULL)
		return -1;

	returnCode = xmlNanoHTTPReturnCode(ctxt);
	if (returnCode >= 300) {
		xmlNanoHTTPClose(ctxt);
		if (content_type != NULL)
			xmlFree(content_type);
		return returnCode;
	} /* code >= 300 */

	while ( (len = xmlNanoHTTPRead(ctxt, buffer, sizeof(buffer))) > 0 )
		fwrite(buffer, len, sizeof(xmlChar), outfp);

	xmlNanoHTTPClose(ctxt);

	return returnCode;
}; /* ndPostPrint(...) */

/*
 * DELETE
 */
int ndDelete(char * url, ndAuthCtxtPtr auth, char * token) {
	int code;
	void * ctxt;
	char if_header[ND_HEADER_LINE_MAX];

	if (url == NULL)
		return -1;

	if_header[0] = '\0';
	if (token)
		if ( snprintf(if_header, sizeof(if_header),
						"If: <%s> (<%s>)\r\n", url, token)
				>= (int) sizeof(if_header) )
		
			return -1;

	ctxt = ndHTTPMethod(url, auth, "DELETE", NULL, NULL,
							token ? if_header : NULL, 0);
	if (ctxt == NULL)
		return(-1);

	code = xmlNanoHTTPReturnCode(ctxt);
	xmlNanoHTTPClose(ctxt);

	return code;
}; /* ndDelete(...) */

ndLockInfoPtr nd_parse_lock_answer(xmlNodePtr cur) {
	ndLockInfoPtr ret = NULL;

	if ( nd_dav_name_equal(cur, "prop") ) {
		ret = NULL;
		cur = cur->children;
		while (cur != NULL) {
			if ( nd_dav_name_equal(cur, "lockdiscovery") )
				ret = nd_parse_lockdiscovery(cur);
			cur = cur->next;
		} /* while */
	} /* case prop */

	return ret;
}; /* ndLockInfoPtr(xmlNodePtr) */

int nd_lock_request(char * url, ndAuthCtxtPtr auth, int	depth,
					char * owner, int scope, char * timeout,
					xmlBufferPtr * buf_return)
{
	char lock_request[ND_REQUEST_MAX];
	char scope_str[16];
	char hstr[ND_HEADER_LINE_MAX];
	const char locktype_string[] =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
		"<D:lockinfo xmlns:D='DAV:'>\n"
		"\t<D:lockscope><D:%s/></D:lockscope>\n"
		"\t<D:locktype><D:write/></D:locktype>\n"
		"\t<D:owner>\n"
		"\t\t<D:href>%s</D:href>\n"
		"\t</D:owner>\n"
		"</D:lockinfo>\n";

	if (! owner)
		owner = "unknown";

	switch (scope) {
		case ND_LOCK_SCOPE_EXCLUSIVE:
			sprintf(scope_str, "exclusive");
			break;
		case ND_LOCK_SCOPE_SHARED:
			sprintf(scope_str, "shared");
			break;
		default:
			sprintf(scope_str, "none");
			break;
	}
	if ( snprintf(lock_request, sizeof(lock_request),
					locktype_string, scope_str, owner)
			>= (int) sizeof(lock_request) )
		return -1;

	/* Depth 1 is forbidden.  */
	if (depth != ND_DEPTH_0 && depth != ND_DEPTH_INFINITE)
		depth = ND_DEPTH_0;

	if (timeout) {
		if ( snprintf(hstr, sizeof(hstr),
					"Timeout: %s\r\n%s", timeout, depth_header[depth])
				>= (int) sizeof(hstr) )
			return -1;
	} else
		sprintf(hstr, "%s", depth_header[depth]);

	return nd_dav_request("LOCK", url, auth, hstr, lock_request,
							strlen(lock_request), buf_return);
}; /* nd_lock_request(...) */

int ndLock(char * url, ndAuthCtxtPtr auth, int depth, char * owner,
			int scope, char * timeout, ndLockInfoPtr * li_return)
{
	int code;
	ndLockInfoPtr ret;
	xmlDocPtr doc;
	xmlBufferPtr buf = NULL;

	code = nd_lock_request(url, auth, depth, owner, scope, timeout, &buf);
	if (buf != NULL) {
		doc = xmlParseMemory((char *) xmlBufferContent(buf),
								xmlBufferLength(buf));
		if (doc == NULL) {
			xmlBufferFree(buf);
			return -1;
		}

		ret = nd_parse_lock_answer(doc->children);
		xmlFreeDoc(doc);
		xmlBufferFree(buf);
		*li_return = ret;
	}

	return code;
}; /* ndLock(...) */

int ndUnlock(char * url, ndAuthCtxtPtr auth, int depth, char * token)
{
	int code;
	void * ctxt;
	char hstr[ND_HEADER_LINE_MAX];

	if (depth < ND_DEPTH_0 || depth > ND_DEPTH_INFINITE)
		depth = ND_DEPTH_0;

	if (url == NULL || token == NULL)
		return -1;
	if ( snprintf(hstr, sizeof(hstr),
				"Lock-token: <%s>\r\n%s", token, depth_header[depth])
			>= (int) sizeof(hstr) )
		return -1;

	ctxt = ndHTTPMethod(url, auth, "UNLOCK", NULL, NULL, hstr, 0);
	if (ctxt == NULL)
		return -1;

	code = xmlNanoHTTPReturnCode(ctxt);
	xmlNanoHTTPClose(ctxt);

	return code;
}; /* ndUnlock(...) */

int ndMkCol(char * url, ndAuthCtxtPtr auth, char * token) {
	int code;
	void * ctxt;
	char if_header[ND_HEADER_LINE_MAX];

	if (url == NULL)
		return -1;
	if_header[0] = '\0';

	if (token)
		if ( snprintf(if_header, sizeof(if_header),
						"If: <%s> (<%s>)\r\n", url, token)
				>= (int) sizeof(if_header) )
			return -1;

	ctxt = ndHTTPMethod(url, auth, "MKCOL", NULL, NULL,
					 token ? if_header : NULL, 0);
	if (ctxt == NULL)
		return -1;

	code = xmlNanoHTTPReturnCode(ctxt);
	xmlNanoHTTPClose(ctxt);

	return code;
}; /* ndMkCol(char *, ndAuthCtxtPtr, char *) */

int ndGet(char * url, ndAuthCtxtPtr auth, char **ct_return,
			xmlBufferPtr * buf_return)
{
	int len, returnCode;
	void * ctxt;
	char * contentType;
	xmlChar buffer[1024];
	xmlBufferPtr output;

	if (url == NULL)
		return -1;

	ctxt = ndHTTPMethod(url, auth, "GET", NULL, &contentType, NULL, 0);
	if (ctxt == NULL)
		return -1;

	returnCode = xmlNanoHTTPReturnCode(ctxt);
	if (returnCode >= 300) {
		xmlNanoHTTPClose(ctxt);
		if (contentType != NULL)
			xmlFree(contentType);
		return returnCode;
	}
	*ct_return = contentType;

	output = xmlBufferCreate();
	if (output == NULL)
		return -1;
	while ( (len = xmlNanoHTTPRead(ctxt, buffer, sizeof(buffer))) > 0 )
		xmlBufferAdd(output, buffer, len);

	xmlNanoHTTPClose(ctxt);
	*buf_return = output;

	return returnCode;
}; /* ndGet(...) */

int ndGetPrint(char * url, ndAuthCtxtPtr auth, char **ct_return,
				FILE * outfp)
{
	int len, returnCode;
	void * ctxt;
	char * contentType = NULL;
	xmlChar buffer[32];

	if (url == NULL)
		return -1;

	ctxt = ndHTTPMethod(url, auth, "GET", NULL, &contentType, NULL, 0);
	if (ctxt == NULL)
		return -1;

	returnCode = xmlNanoHTTPReturnCode(ctxt);
	if (returnCode >= 300) {
		xmlNanoHTTPClose(ctxt);
		if (contentType != NULL)
			xmlFree(contentType);
		return returnCode;
	}
	*ct_return = contentType;

	while ( (len = xmlNanoHTTPRead(ctxt, buffer, sizeof(buffer))) > 0 )
		fwrite(buffer, len, sizeof(xmlChar), outfp);

	xmlNanoHTTPClose(ctxt);

	return returnCode;
}; /* ndGetPrint(...) */

int ndMove(char * url, ndAuthCtxtPtr auth, char * dest_url,
			int overwrite, char * lock_token)
{
	int code;
	void * ctxt;
	char hstr[ND_HEADER_LINE_MAX*3];
	char hstr_1[ND_HEADER_LINE_MAX];

	if (url == NULL || dest_url == NULL)
		return -1;
	if ( snprintf(hstr, sizeof(hstr),
				"Destination: %s\r\nOverwrite: %c\r\n", dest_url,
				overwrite ? 'T' : 'F')
			>= (int) sizeof(hstr) )
		return -1;

	if (lock_token) {
		if ( snprintf(hstr_1, sizeof(hstr_1),
					"If: <%s> (<%s>)\r\n", dest_url, lock_token)
				>= (int) sizeof(hstr_1) )

			return -1;
			if ( (strlen(hstr) + strlen(hstr_1) + 1)
					> sizeof(hstr) )
				return -1;
			strcat(hstr, hstr_1);
	}
	ctxt = ndHTTPMethod(url, auth, "MOVE", NULL, NULL, hstr, 0);
	if (ctxt == NULL)
		return(-1);

	code = xmlNanoHTTPReturnCode(ctxt);
	xmlNanoHTTPClose(ctxt);

	return code;
}; /* ndMove(...) */

int ndCopy(char * url, ndAuthCtxtPtr auth, char * dest_url,
			int overwrite, char * lock_token)
{
	int code;
	void * ctxt;
	char hstr[ND_HEADER_LINE_MAX*3];
	char hstr_1[ND_HEADER_LINE_MAX];

	if (url == NULL || dest_url == NULL)
		return -1;
	if ( snprintf(hstr, sizeof(hstr),
				"Destination: %s\r\nOverwrite: %c\r\n", dest_url,
				overwrite ? 'T' : 'F') >= (int) sizeof(hstr) )
		return -1;

	if (lock_token) {
			if ( snprintf(hstr_1, sizeof(hstr_1),
						"If: <%s> (<%s>)\r\n", dest_url, lock_token)
					>= (int) sizeof(hstr_1) )
				return -1;

			if ( (strlen(hstr) + strlen(hstr_1) + 1)
					> sizeof(hstr) )
				return -1;
			strcat(hstr, hstr_1);
	}
	ctxt = ndHTTPMethod(url, auth, "COPY", NULL, NULL, hstr, 0);
	if (ctxt == NULL)
		return(-1);

	code = xmlNanoHTTPReturnCode(ctxt);
	xmlNanoHTTPClose(ctxt);

	return code;
}; /* ndCopy(...) */

char * ndReasonPhrase(int code) {
	switch (code) {
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
}; /* ndReasonPhrase(int) */
