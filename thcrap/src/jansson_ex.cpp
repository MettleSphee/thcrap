/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Additional JSON-related functions.
  */

#include "thcrap.h"
#include <array>

#pragma warning(push)
#pragma warning(disable:4146)
#include <json5pp.hpp>
#pragma warning(pop)

json_t* json_decref_safe(json_t *json)
{
	if(json && json->refcount != (size_t)-1 && --json->refcount == 0) {
        json_delete(json);
		return NULL;
	}
	return json;
}

size_t json_hex_value(json_t *val)
{
	const char *str = json_string_value(val);
	if(str) {
		return str_address_value(str, NULL, NULL);
	}
	return (size_t)json_integer_value(val);
}

int json_array_set_new_expand(json_t *arr, size_t ind, json_t *value)
{
	size_t arr_size = json_array_size(arr);
	if(ind >= arr_size) {
		int ret = 0;
		size_t i;
		for(i = arr_size; i <= ind; i++) {
			ret = json_array_append_new(arr, value);
		}
		return ret;
	} else {
		return json_array_set_new(arr, ind, value);
	}
}
int json_array_set_expand(json_t *arr, size_t ind, json_t *value)
{
	return json_array_set_new_expand(arr, ind, json_incref(value));
}

size_t json_array_get_hex(json_t *arr, const size_t ind)
{
	json_t *val = json_array_get(arr, ind);
	if(val) {
		size_t ret = json_hex_value(val);
		if(json_is_string(val)) {
			// Rewrite the JSON value
			json_array_set_new(arr, ind, json_integer(ret));
		}
		return ret;
	}
	return 0;
}

const char* json_array_get_string(const json_t *arr, const size_t ind)
{
	return json_string_value(json_array_get(arr, ind));
}

const char* json_array_get_string_safe(const json_t *arr, const size_t ind)
{
	const char *ret = json_array_get_string(arr, ind);
	if(!ret) {
		ret = "";
	}
	return ret;
}

size_t json_flex_array_size(const json_t *json)
{
	return json ? (json_is_array(json) ? json_array_size(json) : 1) : 0;
}

json_t *json_flex_array_get(json_t *flarr, size_t ind)
{
	return json_is_array(flarr) ? json_array_get(flarr, ind) : flarr;
}

const char* json_flex_array_get_string_safe(json_t *flarr, size_t ind)
{
	if(json_is_array(flarr)) {
		const char *ret = json_array_get_string(flarr, ind);
		return ret ? ret : "";
	} else if(json_is_string(flarr)) {
		return ind == 0 ? json_string_value(flarr) : "";
	}
	return NULL;
}

json_t* json_object_get_create(json_t *object, const char *key, json_type type)
{
	json_t *ret = json_object_get(object, key);
	if(!ret && object) {
		// This actually results in nicer assembly than using the ternary operator!
		json_t *new_obj = NULL;
		switch(type) {
			case JSON_OBJECT:
				new_obj = json_object();
				break;
			case JSON_ARRAY:
				new_obj = json_array();
				break;
			default:
				new_obj = json_null();
				break;
		}
		json_object_set_new(object, key, new_obj);
		return new_obj;
	}
	return ret;
}

json_t* json_object_numkey_get(const json_t *object, const json_int_t key)
{
	char key_str[DECIMAL_DIGITS_BOUND(key) + 1];
	snprintf(key_str, sizeof(key_str), "%lld", key);
	return json_object_get(object, key_str);
}

size_t json_object_get_hex(json_t *object, const char *key)
{
	json_t *val = json_object_get(object, key);
	if(val) {
		size_t ret = json_hex_value(val);
		if(json_is_string(val)) {
			 // Rewrite the JSON value
			json_object_set_new_nocheck(object, key, json_integer(ret));
		}
		return ret;
	}
	return 0;
}

const char* json_object_get_string(const json_t *object, const char *key)
{
	if(!key) {
		return NULL;
	}
	return json_string_value(json_object_get(object, key));
}

const char* json_object_get_string_copy(const json_t *object, const char *key)
{
	if(!key) {
		return NULL;
	}
	const char* str = json_string_value(json_object_get(object, key));
	return str ? strdup(str) : NULL;
}

json_t* json_object_merge(json_t *old_obj, json_t *new_obj)
{
	if (!json_object_update_recursive(old_obj, new_obj)) {
		json_decref(new_obj);
		return old_obj;
	}
	else {
		return new_obj;
	}
}

static int __cdecl object_key_compare_keys(const void *key1, const void *key2)
{
	return strcmp(*(const char **)key1, *(const char **)key2);
}

json_t* json_object_get_keys_sorted(const json_t *object)
{
	json_t *ret = NULL;
	if(object) {
		size_t size = json_object_size(object);
		VLA(const char*, keys, size);
		size_t i;
		void *iter = json_object_iter((json_t *)object);

		if(!keys) {
			return NULL;
		}

		i = 0;
		while(iter) {
			keys[i] = json_object_iter_key(iter);
			iter = json_object_iter_next((json_t *)object, iter);
			i++;
		}

		qsort((void*)keys, size, sizeof(const char *), object_key_compare_keys);

		ret = json_array();
		for(i = 0; i < size; i++) {
			json_array_append_new(ret, json_string(keys[i]));
		}
		VLA_FREE(keys);
	}
	return ret;
}

template <typename T, size_t N> T json_tuple_value(
	const json_t* arr, const std::array<const stringref_t, N> value_names
)
{
	T ret;
	if(json_array_size(arr) != N) {
		constexpr stringref_t ERR_FMT = "Must be specified as a JSON array in [%s] format.";
		constexpr stringref_t SEP = ", ";
		size_t allnames_len = SEP.length() * (N - 1);
		for(auto &i : value_names) {
			allnames_len += i.length();
		}
		VLA(char, allnames, allnames_len + 1);
		defer({ VLA_FREE(allnames); });

		char *p = allnames;
		for(int i = 0; i < (int)(N) - 1; i++) {
			p = stringref_copy_advance_dst(p, value_names[i]);
			p = stringref_copy_advance_dst(p, SEP);
		}
		p = stringref_copy_advance_dst(p, value_names[N - 1]);

		ret.err.resize(ERR_FMT.length() + allnames_len + 1);
		sprintf(&ret.err[0], ERR_FMT.data(), allnames);
		return ret;
	}
	for(unsigned int i = 0; i < N; i++) {
		auto coord_j = json_array_get(arr, i);
		bool failed = !json_is_integer(coord_j);
		if(!failed) {
			ret.v.c[i] = (float)json_integer_value(coord_j);
			failed = (ret.v.c[i] < 0.0f);
		}
		if(failed) {
			auto ERR_FMT = "Coordinate #%u (%s) must be a positive JSON integer.";
			ret.err.resize(snprintf(NULL, 0, ERR_FMT, i + 1, value_names[i]));
			sprintf(&ret.err[0], ERR_FMT, i + 1, value_names[i]);
			return ret;
		}
	}
	return ret;
}

json_vector2_t json_vector2_value(const json_t *arr)
{
	return json_tuple_value<json_vector2_t, 2>(arr,
		{ "X", "Y" }
	);
}

json_xywh_t json_xywh_value(const json_t *arr)
{
	return json_tuple_value<json_xywh_t, 4>(arr,
		{ "X", "Y", "width", "height" }
	);
}

json_t *json5_loadb(const void *buffer, size_t size, char **error)
{
	if (error) {
		*error = nullptr;
	}

	json5pp::value json5;
	try {
		json5pp::impl::imemstream istream(buffer, size);
		json5 = json5pp::parse5(istream, false);
	}
	catch (json5pp::syntax_error e) {
		if (error) {
			*error = strdup(e.what());
		}
		return nullptr;
	}

	std::string json_string = json5.stringify();

	json_t *jansson = json_loadb(json_string.c_str(), json_string.size(), 0, nullptr);
	return jansson;
}

json_t* json_load_file_report(const char *json_fn)
{
	size_t json_size;
	const unsigned char utf8_bom[] = { 0xef, 0xbb, 0xbf };
	const unsigned char utf16le_bom[] = { 0xff, 0xfe };
	char *converted_buffer = nullptr;
	char *error = nullptr;
	json_t *ret;
	int msgbox_ret;
	void* file_buffer;
	char *json_buffer;

start:
	msgbox_ret = 0;
	file_buffer = file_read(json_fn, &json_size);
	json_buffer = (char*)file_buffer;

	if (!json_buffer || !json_size) {
		return NULL;
	}

	auto skip_bom = [&json_buffer, &json_size](const unsigned char *bom, size_t bom_len) {
		if (json_size > bom_len && !memcmp(json_buffer, bom, bom_len)) {
			json_buffer += bom_len;
			json_size -= bom_len;
			return true;
		}
		return false;
	};

	if (!skip_bom(utf8_bom, sizeof(utf8_bom))) {
		// Convert UTF-16LE to UTF-8.
		// NULL bytes do not count as significant whitespace in JSON, so
		// they can indeed be used in the absence of a BOM. (In fact, the
		// JSON RFC 4627 Chapter 3 explicitly mentions this possibility.)
		if (
			skip_bom(utf16le_bom, sizeof(utf16le_bom))
			|| (json_size > 2 && json_buffer[1] == '\0')
			) {
			auto converted_len = json_size * UTF8_MUL;
			converted_buffer = (char *)malloc(converted_len);
			json_size = WideCharToMultiByte(
				CP_UTF8, 0, (const wchar_t *)json_buffer, json_size / 2,
				converted_buffer, converted_len, NULL, NULL
			);
			json_buffer = converted_buffer;
		}
	}
	ret = json5_loadb(json_buffer, json_size, &error);
	if (!ret) {
		msgbox_ret = log_mboxf(NULL, MB_RETRYCANCEL | MB_ICONSTOP,
			"JSON parsing error: %s\n"
			"\n"
			"(%s)",
			error, json_fn
		);
	}
	SAFE_FREE(converted_buffer);
	SAFE_FREE(file_buffer);
	SAFE_FREE(error);

	if (msgbox_ret == IDRETRY) {
		goto start;
	}

	return ret;
}

void json_dump_log(const json_t *json, size_t flags)
{
	char* str = json_dumps(json, flags);
	log_print(str);
	free(str);
}

size_t json_string_expression_value(json_t* json) {
	size_t ret = 0;
	if (json_is_string(json)) {
		(void)eval_expr(json_string_value(json), '\0', &ret, NULL, NULL);
	}
	return ret;
}

enum : uint8_t {
  //JEVAL_DEFAULT		= 0b00000,

	JEVAL_TYPE_MASK		= 0b00011,
	JEVAL_BOOL			= 0b00000,
	JEVAL_INTEGER		= 0b00001,
	JEVAL_REAL			= 0b00010,
	JEVAL_NUMBER		= 0b00011,

	JEVAL_MODE_MASK		= 0b11100,
  //JEVAL_LENIENT		= 0b00000,
  //JEVAL_STRICT		= 0b00100,
  //JEVAL_USE_EXPRS		= 0b00000,
  //JEVAL_NO_EXPRS		= 0b01000,
  //JEVAL_INT_TRUNCATE	= 0b00000,
  //JEVAL_INT_RANGE_ERR	= 0b10000
};

// TODO: Would it be beneficial to convert to a
// template so that some of the flags can be
// processed at compiletime with an if constexpr?
static inline jeval_error_t json_evaluate(json_t* json, uint8_t eval_config, void* out) {
	if (json); else return JEVAL_NULL_PTR;
	#define eval_type (eval_config & JEVAL_TYPE_MASK)
	const bool strict = eval_config & JEVAL_STRICT;

#define SetOutValues(bool_val, int_val, real_val) \
eval_type == JEVAL_INTEGER ? *(json_int_t*)out = int_val : \
eval_type == JEVAL_BOOL ? *(bool*)out = bool_val : \
*(double*)out = real_val;

	switch (json_type jtype = json_typeof(json)) {
		case JSON_OBJECT: case JSON_NULL: case JSON_ARRAY:
			if (strict) return JEVAL_ERROR_STRICT_TYPE_MISMATCH;
			SetOutValues(false, 0, 0.0);
			return JEVAL_SUCCESS;
		case JSON_FALSE: {
			if (strict & (eval_type != JEVAL_BOOL)) return JEVAL_ERROR_STRICT_TYPE_MISMATCH;
			SetOutValues(false, 0, 0.0);
			return JEVAL_SUCCESS;
		}
		case JSON_TRUE: {
			if (strict & (eval_type != JEVAL_BOOL)) return JEVAL_ERROR_STRICT_TYPE_MISMATCH;
			SetOutValues(true, 1, 1.0);
			return JEVAL_SUCCESS;
		}
		case JSON_INTEGER: {
			if (strict & !((eval_type == JEVAL_INTEGER) | (eval_type == JEVAL_NUMBER))) return JEVAL_ERROR_STRICT_TYPE_MISMATCH;
			const json_int_t int_value = json_integer_value(json);
			SetOutValues((bool)int_value, int_value, (double)int_value);
			return JEVAL_SUCCESS;
		}
		case JSON_REAL: {
			if (strict & !((eval_type == JEVAL_REAL) | (eval_type == JEVAL_NUMBER))) return JEVAL_ERROR_STRICT_TYPE_MISMATCH;
			const double real_value = json_real_value(json);
			SetOutValues((bool)real_value, (json_int_t)real_value, real_value);
			return JEVAL_SUCCESS;
		}
		case JSON_STRING: {
			if (!(eval_config & JEVAL_NO_EXPRS)); else return JEVAL_ERROR_STRING_NO_EXPRS;
			if (strict & (eval_type == JEVAL_REAL)) return JEVAL_ERROR_STRICT_TYPE_MISMATCH;
			const size_t expr_value = json_string_expression_value(json);
			SetOutValues((bool)expr_value, (json_int_t)expr_value, (double)expr_value);
			return JEVAL_SUCCESS;
		}
	}
	return JEVAL_ERROR_STRICT_TYPE_MISMATCH;

#undef SetOutValues
}

[[nodiscard]] jeval_error_t json_eval_bool(json_t* val, bool* out, jeval_flags_t flags) {
	return json_evaluate(val, JEVAL_BOOL | (flags & JEVAL_MODE_MASK), out);
}

[[nodiscard]] jeval_error_t json_eval_int(json_t* val, size_t* out, jeval_flags_t flags) {
	json_int_t temp;
	jeval_error_t ret = json_evaluate(val, JEVAL_INTEGER | (flags & JEVAL_MODE_MASK), &temp);
	if (ret == JEVAL_SUCCESS) {
		if ((flags & JEVAL_INT_RANGE_ERR) && temp > SIZE_MAX) {
			ret = JEVAL_OUT_OF_RANGE;
		}
		else {
			*out = (size_t)temp;
		}
	}
	return ret;
}

[[nodiscard]] jeval_error_t json_eval_int64(json_t* val, json_int_t* out, jeval_flags_t flags) {
	return json_evaluate(val, JEVAL_INTEGER | (flags & JEVAL_MODE_MASK), out);
}

[[nodiscard]] jeval_error_t json_eval_real(json_t* val, double* out, jeval_flags_t flags) {
	return json_evaluate(val, JEVAL_REAL | (flags & JEVAL_MODE_MASK), out);
}

[[nodiscard]] jeval_error_t json_eval_number(json_t* val, double* out, jeval_flags_t flags) {
	return json_evaluate(val, JEVAL_NUMBER | (flags & JEVAL_MODE_MASK), out);
}


[[nodiscard]] jeval_error_t json_object_get_eval_bool(json_t* object, const char* key, bool* out, jeval_flags_t flags) {
	return json_eval_bool(json_object_get(object, key), out, flags);
}

[[nodiscard]] jeval_error_t json_object_get_eval_int(json_t* object, const char* key, size_t* out, jeval_flags_t flags) {
	return json_eval_int(json_object_get(object, key), out, flags);
}

[[nodiscard]] jeval_error_t json_object_get_eval_int64(json_t* object, const char* key, json_int_t* out, jeval_flags_t flags) {
	return json_eval_int64(json_object_get(object, key), out, flags);
}

[[nodiscard]] jeval_error_t json_object_get_eval_real(json_t* object, const char* key, double* out, jeval_flags_t flags) {
	return json_eval_real(json_object_get(object, key), out, flags);
}

[[nodiscard]] jeval_error_t json_object_get_eval_number(json_t* object, const char* key, double* out, jeval_flags_t flags) {
	return json_eval_number(json_object_get(object, key), out, flags);
}


bool json_eval_bool_default(json_t* val, bool default_ret, jeval_flags_t flags) {
	bool ret = default_ret;
	(void)json_eval_bool(val, &ret, flags);
	return ret;
}

size_t json_eval_int_default(json_t* val, size_t default_ret, jeval_flags_t flags) {
	size_t ret = default_ret;
	(void)json_eval_int(val, &ret, flags);
	return ret;
}

json_int_t json_eval_int64_default(json_t* val, json_int_t default_ret, jeval_flags_t flags) {
	json_int_t ret = default_ret;
	(void)json_eval_int64(val, &ret, flags);
	return ret;
}

double json_eval_real_default(json_t* val, double default_ret, jeval_flags_t flags) {
	double ret = default_ret;
	(void)json_eval_real(val, &ret, flags);
	return ret;
}

double json_eval_number_default(json_t* val, double default_ret, jeval_flags_t flags) {
	double ret = default_ret;
	(void)json_eval_number(val, &ret, flags);
	return ret;
}


bool json_object_get_eval_bool_default(json_t* object, const char* key, bool default_ret, jeval_flags_t flags) {
	bool ret = default_ret;
	(void)json_eval_bool(json_object_get(object, key), &ret, flags);
	return ret;
}

size_t json_object_get_eval_int_default(json_t* object, const char* key, size_t default_ret, jeval_flags_t flags) {
	size_t ret = default_ret;
	(void)json_eval_int(json_object_get(object, key), &ret, flags);
	return ret;
}

json_int_t json_object_get_eval_int64_default(json_t* object, const char* key, json_int_t default_ret, jeval_flags_t flags) {
	json_int_t ret = default_ret;
	(void)json_eval_int64(json_object_get(object, key), &ret, flags);
	return ret;
}

double json_object_get_eval_real_default(json_t* object, const char* key, double default_ret, jeval_flags_t flags) {
	double ret = default_ret;
	(void)json_eval_real(json_object_get(object, key), &ret, flags);
	return ret;
}

double json_object_get_eval_number_default(json_t* object, const char* key, double default_ret, jeval_flags_t flags) {
	double ret = default_ret;
	(void)json_eval_number(json_object_get(object, key), &ret, flags);
	return ret;
}
