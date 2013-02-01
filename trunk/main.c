/*
 * main.c - a terminal interface of ndav.
 *
 * Copyright (c) 2002 Yuuichi Teranishi <teranisi@gohome.org>
 * For license terms, see the file COPYING in this directory.
 *
 * Copyright (C) 2009, 2012 Mats Erik Andersson <meand@users.berlios.de>
 *
 * $Id$
 *
 * vim: set ts=4 sw=4:
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>
#endif /* HAVE_SYS_CDEFS_H */

#include "ndav.h"

#include <stdarg.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_LIBGEN_H
# include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#ifdef HAVE_GETOPT_H
# define _GNU_SOURCE
# include <getopt.h>
#endif /* HAVE_GETOPT_H */

#include <libxml/parser.h>
#include <libxml/nanohttp.h>

#include <errno.h>

#define RETURNED_AN_ERROR(code)	( ( (code) < 0 ) || ( (code) > 300 ) )

static int format = ND_PRINT_AS_HEADER;
static int debug = 0;

ndPropPtr propreq = NULL;

static const char optstring[] = "a:c:de:fg:hi:klm:no:p:qrs:t:uvA:DFN:P:ST:V";

static const struct option long_options[] = {
	{"copy",	required_argument, 0, 'c'},
	{"put",		required_argument, 0, 'p'},
	{"move",	required_argument, 0, 'm'},
	{"delete",	no_argument, 0, 'd'},
	{"get-prop",	required_argument, 0, 'g'},
	{"edit-prop",	required_argument, 0, 'e'},
	{"namespace",	required_argument, 0, 'N'},
	{"post",	required_argument, 0, 'P'},
	{"content-type",	required_argument, 0, 'T'},
	{"find",	no_argument, 0, 'F'},
	{"force",	no_argument, 0, 'f'},
	{"prop-name",	no_argument, 0, 'n'},
	{"lock",	no_argument, 0, 'l'},
	{"unlock",	no_argument, 0, 'u'},
	{"recursive",	no_argument, 0, 'r'},
	{"mkcol",	no_argument, 0, 'k'},
	{"s-expr",	no_argument, 0, 'S'},
	{"help",	no_argument, 0, 'h'},
	{"debug",	no_argument, 0, 'D'},
	{"owner",	required_argument, 0, 'o'},
	{"scope",	required_argument, 0, 's'},
	{"timeout",	required_argument, 0, 'i'},
	{"token",	required_argument, 0, 't'},
	{"auth",	required_argument, 0, 'a'},
	{"proxy-auth",	required_argument, 0, 'A'},
	{"quiet",	no_argument,	0,	'q'},
	{"verbose",	no_argument,	0,	'v'},
	{"version",	no_argument,	0,	'V'},
	{0, 0, 0, 0}
};

void error_exit(int format, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	if (fmt != NULL) {
		if ( NDAV_PRINT_SEXP(format) ) {
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
	exit(EXIT_FAILURE);
}; /* error_exit(...) */

#define NDAV_SHORT_USAGE "%s version %s\n"		\
	"usage: %s [-AacdeFfgiklmNnoPpqrSsTtuVv] url\n\n"	\
	"Without options, a GET is implied.\n"

#define ND_USAGE "%s version %s\n"\
	"usage: %s [options] url\n\n"\
	"If no option is given, the http-action GET is implied.\n\n"\
	"Actions:\n\
	-c|--copy <orig_url>\n\
		Copy 'orig_url' to 'url'.\n\
	-d|--delete\n\
		Delete 'url'. Use a locking token if also '-t' is specified. \n\
	-e|--edit-prop <name=value>\n\
		Edit the property 'name'. Its value is changed to 'value'.\n\
		An empty value, or missing equal character, results in the\n\
		property being removed.\n\
	-F|--find\n\
		Query available properties of 'url' using PROPFIND.\n\
	-g|--get-prop <name>\n\
		Request the property named 'name'.\n\
	-k|--mkcol\n\
		Create the collection 'url', i.e., MKCOL 'url'.\n\
	-l|--lock\n\
		Put a lock on 'url', i.e., LOCK 'url'.\n\
	-m|--move <orig_url>\n\
		Move 'orig_url' to 'url'.\n\
	-n|--prop-name\n\
		Retrieve all available property names of the specified url.\n\
		This displays properties without value, but with name spaces.\n\
	-p|--put-file <file>\n\
		Write the contents of 'file' to 'url' by a PUT request.\n\
		Use a locking token if also '-t' is specified. \n\
	-P|--post <file>\n\
		Submit the contents of 'file' to 'url' as a POST request.\n\
		The content option '-T' is required.\n\
	-u|--unlock\n\
		Issue UNLOCK for 'url'. A token option '-t' is required.\n\n"\
	"Selection modifiers:\n\n\
	-a|--auth <realm>\n\
		Specify authentication realm for a request.\n\
	-A|--proxy-auth <realm>\n\
		Name the proxy authentication realm for the request.\n\
	-f|--force\n\
		Force overwriting target in copying or moving mode..\n\
	-i|--timeout <timeout>\n\
		Specify locking timeout interval. Default is `Infinite'.\n\
	-N|--namespace <namespace-url>\n\
		State the property's namespace, for '-e' or '-g'.\n\
	-o|--owner <owner>\n\
		Claim the lock's owner. Default is environment variable USER.\n\
	-r|--recursive\n\
		When acting on a collection with either of the operations\n\
		'-v', '-l', or '-u', set the depth to infinity, not one.\n\
	-s|--scope <scope>\n\
		Specify locking scope (`exclusive' or `shared').\n\
		The default is `exclusive'.\n\
	-t|--token <token>\n\
		Use locking token `token'.\n\
	-T|--content-type <content_type>\n\
		Set 'content_type' as Content-Type of the POST request.\n\
		Default is `application/x-www-form-urlencoded'.\n\n"\
	"Report format modifiers:\n\n\
	-D|--debug\n\
		Debug mode.\n\
	-q|--quiet\n\
		Minimal output. (not yet implemented)\n\
	-S|--s-expr\n\
		Print output as s-expressions.\n\
	-v|--verbose\n\
		Report verbosely, displaying keywords, etcetera.\n\
	-V|--version\n\
		Print version.\n"

void print_version(char * prog) {
	fprintf(stdout,
			"%s version %s, with Basic"
#ifdef USE_DIGEST
			" and Digest"
#endif /* USE_DIGEST */
			" authentication.\n",
			basename(prog), VERSION);
}; /* print_version(char *) */

void short_usage(char * prog) {
	fprintf(stderr, NDAV_SHORT_USAGE, PACKAGE, VERSION, basename(prog));
}; /* short_usage(char *) */

void usage(char * prog) {
	fprintf(stdout, ND_USAGE, PACKAGE, VERSION, basename(prog));
}; /* usage(char *) */

void auth_notify(void * ctxt) {
	if ( NDAV_PRINT_SEXP(format) ) {

		int code = xmlNanoHTTPReturnCode(ctxt);

		if ( RETURNED_AN_ERROR(code) )
			;
		else if ( NDAV_PRINT_VERBOSELY(format) )
			fprintf(stdout, "OK\n");
	}
}; /* auth_notify(void *) */

int authenticate( ndAuthParamPtr param, int is_proxy) {
	char user[ND_HEADER_LINE_MAX];
	char *realm = ndAuthParamValue(param, "realm");
	char *name = ndAuthParamValue(param, "name");

	if (realm) {
		fprintf(stderr, "%sUsername for %s realm \"%s\": ",
				is_proxy ? "Proxy " : "",
				name, realm);

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

void null_error_handler(void *ctx, const char *msg, ...) {
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
	char *orig_url = NULL;
	char *content_type = "application/x-www-form-urlencoded";
	char *auth_realm = NULL;
	char *pauth_realm = NULL;
	char *ns = NULL;
	int scope = ND_LOCK_SCOPE_EXCLUSIVE;
	int infinite = 0;
	int force_overwrite = 0;
	ndAuthCtxtPtr auth;

	/* OMIT xml errors. */
	xmlSetGenericErrorFunc(NULL, null_error_handler);

	while ( (optc = getopt_long(argc, argv, optstring, long_options, NULL))
			!= -1 )
	{
		switch (optc) {
			case 'c':	mode = 'c';
						orig_url = optarg;
						break;
			case 'm':	mode = 'm';
						orig_url = optarg;
						break;
			case 'e':	mode = 'e';
						{
							char *value = optarg;
							ndPropPtr prp = ndPropNew();

							/* Locate the equal sign. */
							while (*value && (*value != '='))
								value++;

							/* Capture assigned value,
							 * and crop property name. */
							if (*value)
								*value++ = '\0';

							prp->name = strdup(optarg);
							prp->ns = ns ? strdup(ns) : NULL;
							prp->value = *value ? strdup(value)
												: NULL;
							prp->type = *value ? NDPROP_PATCH
											   : NDPROP_REMOVE;
							prp->next = propreq;

							propreq = prp;

						}
						break;
			case 'F':	mode = 'F';
						break;
			case 'g':	mode = 'F';
						{
							ndPropPtr prp = ndPropNew();

							prp->name = strdup(optarg);
							prp->ns = ns ? strdup(ns) : NULL;
							prp->value = NULL;
							prp->type = NDPROP_FIND;
							prp->next = propreq;

							propreq = prp;
						}
						break;
			case 'l':	mode = 'l';
						timeout = "Infinite";
						scope = ND_LOCK_SCOPE_EXCLUSIVE;
						break;
			case 'i':	timeout = optarg;
						break;
			case 'n':	mode = 'n';
						format |= ND_PRINT_NAMEONLY;
						break;
			case 'o':	owner = optarg;
						break;
			case 'd':	mode = 'd';
						break;
			case 'D':	debug = 1;
						break;
			case 'f':	force_overwrite = 1;
						break;
			case 'S':	format |= ND_PRINT_AS_SEXP;
						break;
			case 'q':	format |= ND_PRINT_QUIETLY;
						break;
			case 'v':	format |= ND_PRINT_VERBOSELY;
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
			case 'V':	print_version(argv[0]);
						exit(EXIT_SUCCESS);
						break;
			case 'h':	/* Long help. */
						usage(argv[0]);
						exit(EXIT_SUCCESS);
						break;
			case '?':	/* Short help and catch-all. */
			default:	short_usage(argv[0]);
						exit(EXIT_FAILURE);
						break;
		}
	}

	url = argv[optind];

	if (url == NULL) {
		fprintf(stderr, "%s: No target URL was specified.\n",
				basename(argv[0]));
		exit(EXIT_FAILURE);
	}

	auth = ndCreateAuthCtxt(authenticate, auth_notify,
								auth_realm, pauth_realm);
	switch (mode) {
		case 'c':	{
						int code;

						code = ndCopy(orig_url, auth, url,
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

						if (propreq && propreq->next == NULL
								&& propreq->ns == NULL)
							propreq->ns = ns;

						code = ndPropPatch(url, auth, propreq, token, &ret);

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

						code = ndMove(orig_url, auth, url,
										force_overwrite, token);
						ndFreeAuthCtxt(auth);

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
#else /* WAIT_FOR_END */
						xmlBufferPtr buf = NULL;
						int code;
						char *ct_return = NULL;

						if (token != NULL)
							error_exit(format, "token is not required");

						code = ndGet(url, auth, &ct_return, &buf);
						ndFreeAuthCtxt(auth);

						if ( RETURNED_AN_ERROR(code) )
		
							error_exit(format, "GET failed, `%s'",
										ndReasonPhrase(code));
	
						if ( buf )
							fprintf(stdout, "%s", xmlBufferContent(buf));
#endif /* WAIT_FOR_END */
					};
					break;
		case 'n':
		case 'F':	{
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

						if (propreq && propreq->next == NULL
								&& propreq->ns == NULL)
							propreq->ns = ns;

						code = ndPropFind(url, auth, propreq, depth,
										  (mode == 'n') ? 1 : 0,
										  &ret);
						ndFreeAuthCtxt(auth);
	
						if (ret)
							ndNodeInfoListPrint(stdout, ret, format);
						else
							error_exit(format, "PROPFIND failed, `%s'",
										ndReasonPhrase(code));
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
									owner ? owner : getenv("USER"),
									scope, timeout, &lock);
						ndFreeAuthCtxt(auth);

						if (lock) {
							ndLockInfoPrint(stdout, lock, format);
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
						xmlBufferPtr buf = xmlBufferCreate();
						int len;
						unsigned char s [1024];
						int ret;
						FILE *fp;

						if ((fp = fopen(infile, "r")) == NULL)
							error_exit(format, "%s, %s", infile,
										strerror(errno));

						while ( (len = fread(s, sizeof(unsigned char),
												sizeof(s), fp))
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
										ndReasonPhrase(ret));
					};
					break;
		case 'p':	{
						int len;
						unsigned char s [1024];
						int code;
						FILE *fp;
						ndNodeInfoPtr ret = NULL;
						xmlBufferPtr buf = xmlBufferCreate();

						if ((fp = fopen(infile, "r")) == NULL)
							error_exit(format, "%s, %s",
										infile, strerror(errno));

						while ( (len = fread(s, sizeof(unsigned char),
												sizeof(s), fp))
								> 0)
							xmlBufferAdd(buf, s, len);

		
						fclose(fp);
						code = ndPut(url, auth,
									(char *) xmlBufferContent(buf),
									xmlBufferLength(buf), token, &ret);
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
