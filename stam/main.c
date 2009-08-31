/*
 * main.c - a terminal interface of nd.
 *
 * Copyright (c) 2002 Yuuichi Teranishi <teranisi@gohome.org>
 * For license terms, see the file COPYING in this directory.
 *
 * Copyright (C) 2009 Mats Erik Andersson <mats.andersson@gisladisker.se>
 *
 * vim: set ts=4 sw=4:
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "nd.h"

#include <stdarg.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <libgen.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <libxml/parser.h>
#include <libxml/nanohttp.h>

#include <errno.h>

#define RETURNED_AN_ERROR(code)	( ( (code) < 0 ) || ( (code) > 300 ) )

int format = ND_PRINT_AS_HEADER;
int debug = 0;

const char optstring[] = "c:de:fg:hi:lm:o:vukrs:t:p:a:A:ST:P:DN:";

void error_exit (int format, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	if (fmt != NULL) {
		if (format == ND_PRINT_AS_SEXP) {
			fprintf(stdout, "(error \"");
			vfprintf(stdout, fmt, ap);
			fprintf(stdout, "\")\n");
		}
		else
		{
			fprintf(stderr, "Error: ");
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		}
	}
	va_end(ap);
	exit(1);
}; /* error_exit(...) */

#define ND_USAGE "%s version %s\n\
usage: %s [options] url\n\
	if no option, GET url.\n\
	-c dest_url\n\
		 COPY url to the dest_url. (Not implemented yet)\n\
	-v\n\
		 View property information of url by PROPFIND.\n\
		 With -g option, only the specified property is displayed.\n\
	-p file\n\
		 Write file content to the url by PUT.\n\
		 Use lock token if -t is specified. \n\
	-g name\n\
		 Specify the property name for -v option.\n\
	-e name=value\n\
		 Edit the property.\n\
		 The property named 'name' is changed to 'value'.\n\
		 If multiple '-e' options are specified, only the first one takes\n\
		 effect.\n\
		 -N is required if namespace of the property is other than 'DAV:'\n\
	-N namespace-url\n\
		 Specify the property namespace URL for -e or -g option.\n\
	-P file\n\
		 POST file content to the url. -T is required.\n\
	-T content_type\n\
		 Use content_type as a Content-Type of the POST request.\n\
		 Default is `application/x-www-form-urlencoded'.\n\
	-d\n\
		 DELETE url. Use lock token if -t is specified. \n\
	-l\n\
		 LOCK url.\n\
	-k\n\
		 MKCOL url.\n\
	-m dest_url\n\
		 MOVE url to the dest_url. (Not implemented yet)\n\
	-o owner\n\
		 Specify lock owner. Default is USER environment variable.\n\
	-r\n\
		 Execute operation by setting depth as infinity. (Not implemented yet)\n\
	-a realm\n\
		 Specify authentication realm for the request.\n\
	-A realm\n\
		 Specify proxy authentication realm for the request.\n\
	-s scope\n\
		 Specify lock scope (`exclusive' or `shared'). Default is `exclusive'.\n\
	-i timeout\n\
		 Specify lock timeout interval. Default is `Infinite'.\n\
	-u\n\
		 UNLOCK url. -t option is required.\n\
	-t token\n\
		 Use lock token `token'.\n\
	-S\n\
		 Print output by s-expression.\n\
	-D\n\
		 Debug mode.\n"

void usage(char * prog) {
	fprintf(stderr, ND_USAGE, PACKAGE, VERSION, prog);
}; /* usage(char *) */

void auth_notify(void * ctxt) {
	if (format == ND_PRINT_AS_SEXP) {

		int code = xmlNanoHTTPReturnCode(ctxt);

		if ( RETURNED_AN_ERROR(code) )
			;
		else
			fprintf(stdout, "OK\n");
	}
}; /* auth_notify(void *) */

int authenticate( ndAuthParamPtr param, int is_proxy) {
	char user[ND_HEADER_LINE_MAX];
	char *realm = ndAuthParamValue(param, "realm");

	if (realm) {
		fprintf(stderr, "%sUsername for %s: ",
						is_proxy ? "Proxy " : "",
						realm);

		if ( fgets(user, ND_HEADER_LINE_MAX, stdin) == NULL )
			return -1;

		if ( user[strlen(user) - 1] == '\n' )
			user[strlen(user) - 1] = '\0';

		ndAuthParamSetValue(param, "user", user);
		ndAuthParamSetValue(param, "password",
		getpass(is_proxy ? "Proxy Password: " : "Password: "));
		return 0;
	}
	return -1;
}; /* authenticate(ndAuthParamPtr, int) */

#undef WAIT_FOR_END

void null_error_handler (void *ctx, const char *msg, ...) {
	/* Empty */
}; /* null_error_handler(...) */

int main(int argc, char * argv[]) {
	int optc;
	int mode = 'g';
	char *url;
	char *token = NULL;
	char *timeout = NULL;
	char *infile = NULL;
	char *owner = NULL;
	char *dest_url = NULL;
	char *content_type = "application/x-www-form-urlencoded";
	char *auth_realm	= NULL;
	char *pauth_realm = NULL;
	char *edit = NULL;
	char *prop = NULL;
	char *ns = NULL;
	int scope = ND_LOCK_SCOPE_EXCLUSIVE;
	int infinite = 0;
	int force_overwrite = 0;
	ndAuthCtxtPtr auth;

	/* OMIT xml errors. */
	xmlSetGenericErrorFunc(NULL, null_error_handler);

	while ( (optc = getopt(argc, argv, optstring)) != -1 )
	{
		switch (optc) {
			case 'c':	mode = 'c';
						dest_url = optarg;
						break;
			case 'm':	mode = 'm';
						dest_url = optarg;
						break;
			case 'e':	mode = 'e';
						edit = optarg;
						break;
			case 'g':	prop = optarg;
						break;
			case 'l':	mode = 'l';
						timeout = "Infinite";
						scope = ND_LOCK_SCOPE_EXCLUSIVE;
						break;
			case 'i':	timeout = optarg;
						break;
			case 'o':	owner = optarg;
						break;
			case 'v':	mode = 'v';
						break;
			case 'd':	mode = 'd';
						break;
			case 'D':	debug = 1;
						break;
			case 'f':	force_overwrite = 1;
						break;
			case 'S':	format = ND_PRINT_AS_SEXP;
						break;
			case 'a':	auth_realm = optarg;
						break;
			case 'A':	pauth_realm = optarg;
						break;
			case 'P':	mode = 'P';
						infile = optarg;
						break;
			case 'T':	content_type = optarg;
						break;
			case 'N':	ns = optarg;
						break;
			case 'u':	mode = 'u';
						break;
			case 'k':	mode = 'k';
						break;
			case 'r':	infinite = 1;
						break;
			case 's':	if (! strcmp("shared", optarg))
							scope = ND_LOCK_SCOPE_SHARED;
						break;
			case 't':	token = optarg;
						break;
			case 'p':	mode = 'p';
						infile = optarg;
						break;
			case 'h':	/* Help and catch-all. */
			case '?':
			default:	usage(argv[0]);
						exit(1);
						break;
		}
	}

	url = argv[optind];

	if (url == NULL) {
		usage(argv[0]);
		exit(1);
	}

	auth = ndCreateAuthCtxt(authenticate, auth_notify,
								auth_realm, pauth_realm);
	switch (mode) {
		case 'c':	{
						int code;

						code = ndCopy(url, auth, dest_url,
										force_overwrite, token);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(code) )
							error_exit(format, "COPY failed, `%s'",
										ndReasonPhrase(code));
					};
					break;
		case 'e':	{
						int code;
						ndNodeInfoPtr ret = NULL;
						char *name = edit;
						char *value = NULL;

						/* Locate the equal sign. */
						for (value = name;
								(*value != '\0') && (*value != '=');
								value++)
							;

						/* Now "*value" is either '\0' or '='. */ 

						if ( *value != '\0' )
							/* Step over the assignment character
							 * and make sure that "char * name"
							 * contains only the property name. */
							*value++ = '\0';

						if ( *value == '\0' )
							/* An empty value string was submitted. */
							value = NULL;

						code = ndPropPatch(url, auth, name, value,
											ns, token, &ret);

						if ( RETURNED_AN_ERROR(code) )
							error_exit(format, "PROPPATCH failed, `%s'",
										ndReasonPhrase(code));
						else
							if (code == 207)
								ndNodeInfoListPrint(stdout, ret, format);
					};
					break;
		case 'm':	{
						int code;

						code = ndMove(url, auth, dest_url,
										force_overwrite, token);
						ndFreeAuthCtxt (auth);

						if ( RETURNED_AN_ERROR(code) )
							error_exit(format, "MOVE failed, `%s'",
										ndReasonPhrase(code));
					};
					break;
		case 'g':	{
#ifndef WAIT_FOR_END
						char *ct_return = NULL;
						int	code;

						if (token != NULL)
							error_exit(format, "token is not required");

						code = ndGetPrint(url, auth, &ct_return, stdout);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(code) )
							error_exit(format, "GET failed, `%s'",
										ndReasonPhrase(code));
#else
						xmlBufferPtr buf = NULL;
						int code;
						char *ct_return = NULL;

						if (token != NULL)
							error_exit(format, "token is not required");

						code = ndGet(url, auth, &ct_return, &buf);
						ndFreeAuthCtxt (auth);

						if ( RETURNED_AN_ERROR(code) )
		
							error_exit(format, "GET failed, `%s'",
										ndReasonPhrase(code));
	
						if ( buf )
							fprintf(stdout, "%s", xmlBufferContent(buf));
#endif
					};
					break;
		case 'v':	{
						ndNodeInfoPtr ret = NULL;
						int code;
						int depth;

						if (token != NULL)
							error_exit(format, "token is not required");

						depth = ( url[strlen(url) - 1] == '/' )
									? ( infinite
											? ND_DEPTH_INFINITE
											: ND_DEPTH_1 )
									: ND_DEPTH_0;

						code = ndPropFind(url, auth, prop, ns, depth, &ret);
						ndFreeAuthCtxt(auth);
	
						if (ret)
							ndNodeInfoListPrint(stdout, ret, format);
						else
							error_exit(format, "PROPFIND failed, `%s'",
										ndReasonPhrase (code));
					};
					break;
		case 'l':	{
						ndLockInfoPtr lock = NULL;
						int code;
						int depth;

						depth = ( url[strlen(url) - 1] == '/' )
									? ( infinite
											? ND_DEPTH_INFINITE
											: ND_DEPTH_1 )
									: ND_DEPTH_0;

						code = ndLock(url, auth, depth,
									owner ? owner : getenv ("USER"),
									scope, timeout, &lock);
						ndFreeAuthCtxt(auth);

						if (lock) {
							ndLockInfoPrint(stdout, lock, format);
							fprintf(stdout, "\n");
						} else
							error_exit(format, "LOCK failed, `%s'",
										ndReasonPhrase(code));
					};
					break;
		case 'u':	{
						int ret;
						int depth;

						if (token == NULL)
							error_exit(format, "token is required");
		
						depth = ( url[strlen(url) - 1] == '/' )
									? ( infinite
											? ND_DEPTH_INFINITE
											: ND_DEPTH_1 )
									: ND_DEPTH_0;

						ret = ndUnlock(url, auth, depth, token);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(ret) )
							error_exit(format, "UNLOCK failed, `%s'",
										ndReasonPhrase(ret));
					};
					break;
		case 'P':	{
						xmlBufferPtr buf = xmlBufferCreate ();
						int len;
						unsigned char s [1024];
						int ret;
						FILE *fp;

						if ((fp = fopen(infile, "r")) == NULL)
							error_exit(format, "%s, %s", infile,
										strerror(errno));

						while ( (len = fread(s, sizeof(unsigned char),
												sizeof (s), fp))
									> 0)
							xmlBufferAdd(buf, s, len);

						fclose(fp);
						ret = ndPostPrint(url, auth,
									(char *) xmlBufferContent(buf),
									xmlBufferLength(buf),
									&content_type, stdout);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(ret) )
							error_exit(format, "POST failed, `%s'",
										ndReasonPhrase(ret));
					};
					break;
		case 'k':	{
						int ret;

						ret = ndMkCol(url, auth, token);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(ret) )
							error_exit(format, "MKCOL failed, `%s'",
										ndReasonPhrase(ret));
					};
					break;
		case 'd':	{
						int ret;

						ret = ndDelete(url, auth, token);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(ret) )
							error_exit(format, "DELETE failed, `%s'",
										ndReasonPhrase (ret));
					};
					break;
		case 'p':	{
						int len;
						unsigned char s [1024];
						int code;
						FILE *fp;
						ndNodeInfoPtr ret = NULL;
						xmlBufferPtr buf = xmlBufferCreate ();

						if ((fp = fopen(infile, "r")) == NULL)
							error_exit(format, "%s, %s",
										infile, strerror (errno));

						while ( (len = fread(s, sizeof(unsigned char),
												sizeof (s), fp))
								> 0)
							xmlBufferAdd(buf, s, len);

		
						fclose(fp);
						code = ndPut(url, auth,
									(char *) xmlBufferContent (buf),
									xmlBufferLength (buf), token, &ret);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(code) )
							error_exit(format, "PUT failed, `%s'",
									ndReasonPhrase(code));
	
						else
							ndNodeInfoListPrint(stdout, ret, format);
					};
					break;
	};
	return 0;
}; /* main(int, char * []) */
