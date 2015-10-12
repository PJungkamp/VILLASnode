
/** Node type: OMA Next Generation Services Interface 10 (NGSI) (FIWARE context broker)
 *
 * This file implements the NGSI context interface. NGSI is RESTful HTTP is specified by
 * the Open Mobile Alliance (OMA).
 * It uses the standard operations of the NGSI 10 context information standard.
 *
 * @see https://forge.fiware.org/plugins/mediawiki/wiki/fiware/index.php/FI-WARE_NGSI-10_Open_RESTful_API_Specification
 * @see http://technical.openmobilealliance.org/Technical/Release_Program/docs/NGSI/V1_0-20120529-A/OMA-TS-NGSI_Context_Management-V1_0-20120529-A.pdf
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 **********************************************************************************/

#include <string.h>
#include <stdio.h>

#include <curl/curl.h>
#include <uuid/uuid.h>
#include <jansson.h>

#include "ngsi.h"
#include "utils.h"

extern struct settings settings;

static json_t * json_uuid()
{
	char eid[37];
	uuid_t uuid;
	
	uuid_generate_time(uuid);
	uuid_unparse_lower(uuid, eid);

	return json_string(eid);
}

static json_t * json_date(struct timespec *ts)
{
	// Example: 2015-09-21T11:42:25+02:00
	char date[64];
	strftimespec(date, sizeof(date), "%FT%T.%u%z", ts);
	
	return json_string(date);
}

static json_t * json_lookup(json_t *array, char *key, char *needle)
{
	size_t ind;
	json_t *obj;

	json_array_foreach(array, ind, obj) {
		json_t *value = json_object_get(obj, key);
		if (value && json_is_string(value)) {
			if (!strcmp(json_string_value(value), needle))
				return obj;
		}
	}
	
	return NULL;
}

struct ngsi_response {
	char *data;
	size_t len;
};

static size_t ngsi_request_writer(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct ngsi_response *mem = (struct ngsi_response *) userp;
 
	mem->data = realloc(mem->data, mem->len + realsize + 1);
	if(mem->data == NULL) /* out of memory! */
		error("Not enough memory (realloc returned NULL)");

	memcpy(&(mem->data[mem->len]), contents, realsize);
	mem->len += realsize;
	mem->data[mem->len] = 0;

	return realsize;
}

static int ngsi_request(CURL *handle, const char *endpoint, const char *operation, json_t *content, json_t **response)
{
	struct ngsi_response chunk = { 0 };
	char *post = json_dumps(content, JSON_INDENT(4));

	char url[128];
	snprintf(url, sizeof(url), "%s/v1/%s", endpoint, operation);
	
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, ngsi_request_writer);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) &chunk);	
	curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, strlen(post));
	curl_easy_setopt(handle, CURLOPT_POSTFIELDS, post);
	
	debug(20, "Request to context broker:\n%s", post);
	
	CURLcode ret = curl_easy_perform(handle);
	if (ret)
		error("HTTP request failed: %s", curl_easy_strerror(ret));

	long code;
	double time;
	curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &time);
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &code);
	
	debug(20, "Request to context broker completed in %.4f seconds", time);
	debug(20, "Response from context broker (code=%ld):\n%s", code, chunk.data);
	
	json_error_t err;
	json_t *resp = json_loads(chunk.data, 0, &err);
	if (!resp)
		error("Received invalid JSON: %s in %s:%u:%u\n%s", err.text, err.source, err.line, err.column, chunk.data);
	
	if (response)
		*response = resp;
	else
		json_decref(resp);
	
	free(post);
	free(chunk.data);

	return code;
}

void ngsi_prepare_context(struct node *n, config_setting_t *mapping)
{	
	struct ngsi *i = n->ngsi;

	i->context     = json_object();

	list_init(&i->mapping, NULL);

	json_t *elements = json_array();
	
	for (int j = 0; j < config_setting_length(mapping); j++) {
		/* Get token */
		config_setting_t *ctoken = config_setting_get_elem(mapping, j);
		const char *stoken = config_setting_get_string(ctoken);
		if (!stoken)
			cerror(mapping, "Invalid token");

		/* Parse token */
		char eid[64], etype[64], aname[64], atype[64];
		if (sscanf(stoken, "%63[^().](%63[^().]).%63[^().](%63[^().])", eid, etype, aname, atype) != 4)
			cerror(ctoken, "Invalid token: '%s'", stoken);

		/* Create entity */
		json_t *attributes; /* borrowed reference */

		json_t *entity = json_lookup(elements, "id", eid);
		if (!entity) {
			entity = json_pack("{ s: s, s: s, s: b }",
				"id",		eid,
				"type",		etype,
				"isPattern", 	0
			);
			json_array_append_new(elements, entity);
		
			attributes = json_array();
			json_object_set_new(entity, "attributes", attributes);
		}
		else {
			if (i->structure == NGSI_CHILDREN)
				cerror(ctoken, "Duplicate mapping for index %u", j);

			attributes = json_object_get(entity, "attributes");
		}

		/* Create attribute for entity */
		if (json_lookup(attributes, "name", aname))
			cerror(ctoken, "Duplicated attribute '%s' in NGSI mapping of node '%s'", aname, n->name);
			
		/* Create Metadata for attribute */
		json_t *metadatas = json_array();

		json_array_append_new(metadatas, json_pack("{ s: s, s: s, s: s+ }",
			"name",  "source",
			"type",  "string",
			"value", "s2ss:", settings.name
		));
		json_array_append_new(metadatas, json_pack("{ s: s, s: s, s: i }",
			"name", "index",
			"type", "integer",
			"value", j
		));
		json_array_append(metadatas, json_pack("{ s: s, s: s, s: s }",
			"name", "timestamp",
			"type", "date",
			"value", ""
		));		

		if (i->structure == NGSI_CHILDREN) {
			json_array_append_new(attributes, json_pack("{ s: s, s: s, s: s }",
				"name", "parentId",
				"type", "uuid",
				"value", eid
			));
				
			json_array_append_new(attributes, json_pack("{ s: s, s: s, s: s }",
				"name", "source",
				"type", "string",
				"value", "measurement"
			));
			
			json_array_append_new(attributes, json_pack("{ s: s, s: s, s: o }",
				"name", "timestamp",
				"type",	"date",
				"value", json_date(NULL)
			));
		}

		json_t *attribute = json_pack("{ s: s, s: s, s: [ ] }",
			"name",		aname,
			"type",		atype,
			"value"
		);

		list_push(&i->mapping, attribute);
		
		json_object_set_new(attribute, "metadatas", metadatas);
		json_array_append_new(attributes, attribute);
	}
	
	json_object_set_new(i->context, "contextElements", elements);
}

int ngsi_init(int argc, char *argv[], struct settings *set)
{
	return curl_global_init(CURL_GLOBAL_ALL);
}

int ngsi_deinit()
{
	curl_global_cleanup();

	return 0;
}

int ngsi_parse(config_setting_t *cfg, struct node *n)
{
	struct ngsi *i = alloc(sizeof(struct ngsi));

	if (!config_setting_lookup_string(cfg, "token", &i->token))
		i->token = NULL; /* disabled by default */

	if (!config_setting_lookup_string(cfg, "endpoint", &i->endpoint))
		cerror(cfg, "Missing NGSI endpoint for node '%s'", n->name);

	if (!config_setting_lookup_bool(cfg, "ssl_verify", &i->ssl_verify))
		i->ssl_verify = 1; /* verify by default */

	if (!config_setting_lookup_float(cfg, "timeout", &i->timeout))
		i->timeout = 1; /* default value */

	const char *structure;
	if (!config_setting_lookup_string(cfg, "structure", &structure))
		i->structure = NGSI_FLAT;
	else {
		if	(!strcmp(structure, "flat"))
			i->structure = NGSI_FLAT;
		else if	(!strcmp(structure, "children"))
			i->structure = NGSI_CHILDREN;
		else
			cerror(cfg, "Invalid NGSI entity structure '%s' for node '%s'", structure, n->name);
	}
	
	n->ngsi = i;

	config_setting_t *mapping = config_setting_get_member(cfg, "mapping");
	if (!mapping || !config_setting_is_array(mapping))
		cerror(cfg, "Missing mapping for node '%s", n->name);

	ngsi_prepare_context(n, mapping);
	
	return 0;
}

char * ngsi_print(struct node *n)
{
	struct ngsi *i = n->ngsi;
	char *buf = NULL;

	return strcatf(&buf, "endpoint=%s, timeout=%.3f secs",
		i->endpoint, i->timeout);
}

int ngsi_open(struct node *n)
{
	char buf[128];
	struct ngsi *i = n->ngsi;

	i->curl = curl_easy_init();
	i->headers = NULL;
	
	if (i->token) {
		snprintf(buf, sizeof(buf), "Auth-Token: %s", i->token);
		i->headers = curl_slist_append(i->headers, buf);
	}
	
	i->headers = curl_slist_append(i->headers, "User-Agent: S2SS " VERSION);
	i->headers = curl_slist_append(i->headers, "Accept: application/json");
	i->headers = curl_slist_append(i->headers, "Content-Type: application/json");
	
	curl_easy_setopt(i->curl, CURLOPT_SSL_VERIFYPEER, i->ssl_verify);
	curl_easy_setopt(i->curl, CURLOPT_TIMEOUT_MS, i->timeout * 1e3);
	curl_easy_setopt(i->curl, CURLOPT_HTTPHEADER, i->headers);
	
	/* Create entity and atributes */
	json_object_set_new(i->context, "updateAction", json_string("APPEND"));

	return ngsi_request(i->curl, i->endpoint, "updateContext", i->context, NULL) == 200 ? 0 : -1;
}

int ngsi_close(struct node *n)
{
	struct ngsi *i = n->ngsi;
	
	/* Delete attributes */
	json_object_set_new(i->context, "updateAction", json_string("DELETE"));
	int code = ngsi_request(i->curl, i->endpoint, "updateContext", i->context, NULL) == 200 ? 0 : -1;
	
	curl_easy_cleanup(i->curl);
	curl_slist_free_all(i->headers);
	
	return code == 200 ? 0 : -1;
}

int ngsi_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{	
	return -1; /** @todo not yet implemented */
}

int ngsi_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct ngsi *i = n->ngsi;
	struct msg *m = &pool[first % poolsize];

	if (cnt > 1)
		error("NGSI nodes only can send a single message at once");

	/* Update context */
	for (int j = 0; j < MIN(i->mapping.length, m->length); j++) {
		json_t *attribute = list_at(&i->mapping, j);
		json_t *values = json_object_get(attribute, "value");
		json_t *metadatas = json_object_get(attribute, "metadatas");

		/* Update timestamp */		
		json_t *metadata_ts = json_lookup(metadatas, "name", "timestamp");
		json_object_set(metadata_ts, "value", json_date(&MSG_TS(m)));
		
		/* Update value */
		json_array_clear(values);
		json_array_append_new(values, json_real(m->data[j].f));
	}
	
	/* Update UUIDs for children structure */
	if (i->structure == NGSI_CHILDREN) {
		json_t *entity, *elements = json_object_get(i->context, "contextElements");
		size_t ind;
		json_array_foreach(elements, ind, entity)
			json_object_set_new(entity, "id", json_uuid());
		
		json_object_set_new(i->context, "updateAction", json_string("APPEND"));
	}
	else
		json_object_set_new(i->context, "updateAction", json_string("UPDATE"));

	json_t *response;
	int code = ngsi_request(i->curl, i->endpoint, "updateContext", i->context, &response);
	if (code != 200)
		error("Failed to NGSI update Context request:\n%s", json_dumps(response, JSON_INDENT(4)));

	return 1;
}

REGISTER_NODE_TYPE(NGSI, "ngsi", ngsi)
