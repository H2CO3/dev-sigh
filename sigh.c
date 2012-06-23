#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <regex.h>
#include <sys/stat.h>
#include <curl/curl.h>

struct regex_range {
	int location; /* These are borrowed from Cocoa */
	int length;
};

#define NOT_FOUND (-1) /* this one also */

char sightext[1024 * 128];
size_t sighpos = 0;

size_t conn_handler(char *buf, size_t count, size_t blocksize, void *ctx)
{
	size_t size = count * blocksize;
	memcpy(sightext + sighpos, buf, size);
	sighpos += size;
	sightext[sighpos] = 0;
	return size;
}

char *replace_str(const char *str, const char *orig, const char *rep)
{
	char buf[4096];
	char tmp[4096];
	char *p;

	strcpy(buf, str);
	while ((p = strstr(buf, orig)) != NULL)
	{
		memcpy(tmp, buf, p - buf);
		memcpy(tmp + (p - buf), rep, strlen(rep));
		strcpy(tmp + (p - buf) + strlen(rep), p + strlen(orig));
		strcpy(buf, tmp);
	}

	return strdup(buf);
}

struct regex_range regex_match(const char *regex, const char *string)
{
	struct regex_range res;

	regex_t regex_obj;
	regmatch_t match;
	int error;

	error = regcomp(&regex_obj, regex, REG_EXTENDED);
	if (error)
	{
		res.location = NOT_FOUND;
		res.length = 0;
		return res;
	}

	error = regexec(&regex_obj, string, 1, &match, 0);
	if (error)
	{
		res.location = NOT_FOUND;
		res.length = 0;
		return res;
	}

	regfree(&regex_obj);
	res.location = match.rm_so;
	res.length = match.rm_eo - match.rm_so;

	return res;
}

int main(int argc, char **argv)
{
	char *fargv[] = { argv[0], NULL };
	char *fenvp[] = { NULL };

	mkfifo("/dev/sigh", 0644);
	FILE *f = fopen("/dev/sigh", "w");

	CURL *curl_hndl = curl_easy_init();
	char url[1024];
	srand(time(NULL));
	int r = rand() % 600 + 1;
	sprintf(url, "http://devsigh.com/sigh/%d", r);
	curl_easy_setopt(curl_hndl, CURLOPT_URL, url);
	curl_easy_setopt(curl_hndl, CURLOPT_WRITEFUNCTION, conn_handler);
	curl_easy_perform(curl_hndl);
	curl_easy_cleanup(curl_hndl);

	/* evil content-dependent info extraction */
	struct regex_range match = regex_match("content-box.*div class\\=\\\"share-outer-box", sightext);

	char text[match.length + 1];
	snprintf(text, match.length - 13 - 30 - 3, "%s", sightext + match.location + 13);
	char *tmp;
	char *final = replace_str(text, "<br />", "\n");
	tmp = replace_str(final, "&quot;", "\"");
	free(final);
	final = replace_str(tmp, "&#39;", "'");
	free(tmp);
	tmp = replace_str(final, "&lt;", "<");
	free(final);
	final = replace_str(tmp, "&gt;", ">");
	free(tmp);
	fprintf(f, "%s\n", final);
	free(final);
	fclose(f);
	remove("/dev/sigh");

	execve(argv[0], fargv, fenvp);

	return 0;
}

