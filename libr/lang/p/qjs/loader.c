// https://github.com/frida/frida-gum/blob/bd6f95d391b198d7d87264ba56f2972efc7298ec/bindings/gumjs/gumquickscriptbackend.c#L259

const char * const package_marker = "📦\n";
const char * const delimiter_marker = "\n✄\n";
const char * const alias_marker = "\n↻ ";

static void r2qjs_dump_obj(JSContext *ctx, JSValueConst val);

static char *r2qjs_normalize_module_name(void* self, JSContext * ctx, const char * base_name, const char * name) {
	if (r_str_startswith (base_name, "./")) {
		return strdup (base_name + 1);
	}
	// R_LOG_INFO ("normalize (%s) (%s)", base_name, name);
	return strdup (base_name);
}

static JSModuleDef *r2qjs_load_module(JSContext *ctx, const char *module_name, void *opaque) {
	if (!strcmp (module_name, "r2papi")) {
		const char *data =  "export var R2Papi = global.R2Papi;\n"\
				    "export var R2PapiShell = global.R2PapiShell;\n"\
				    "export var NativePointer = global.NativePointer;\n"\
				    "export var EsilParser = global.EsilParser;\n"\
				    "export var EsilToken = global.EsilToken;\n"\
				    "export var r2 = global.r2;\n"\
				    "export var R = global.R;\n"\
				    ;
		JSValue val = JS_Eval (ctx, data, strlen (data), module_name,
				JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY);
		if (JS_IsException (val)) {
			JSValue e = JS_GetException (ctx);
			r2qjs_dump_obj (ctx, e);
			return NULL;
		}
		JS_FreeValue (ctx, val);
		return JS_VALUE_GET_PTR (val);
	} else if (!strcmp (module_name, "r2pipe")) {
		const char *data =  "export function open() {\n"\
				    "  return {\n"\
				    "    cmd: r2.cmd,\n"\
				    "    cmdj: r2.cmdj,\n"\
				    "  };\n"\
				    "};\n"\
				    ;
		JSValue val = JS_Eval (ctx, data, strlen (data), module_name,
				JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY);
		if (JS_IsException (val)) {
			JSValue e = JS_GetException (ctx);
			r2qjs_dump_obj (ctx, e);
			return NULL;
		}
		JS_FreeValue (ctx, val);
		return JS_VALUE_GET_PTR (val);
	}
	HtPP *ht = opaque;
	if (!ht) {
		return NULL;
	}
	char *data = ht_pp_find (ht, module_name, NULL);
	if (data) {
		JSValue val = JS_Eval (ctx, data, strlen (data), module_name,
				JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY);
		if (JS_IsException (val)) {
			JSValue e = JS_GetException (ctx);
			r2qjs_dump_obj (ctx, e);
			return NULL;
		}
		// R_LOG_INFO ("loaded (%s)", module_name);
		JS_FreeValue (ctx, val);
		return JS_VALUE_GET_PTR (val);
	}
	R_LOG_ERROR ("Cannot find module (%s)", module_name);
	return NULL;
}

static void r2qjs_modules(JSContext *ctx) {
	JSRuntime *rt = JS_GetRuntime (ctx);
	JS_SetModuleLoaderFunc (rt, (JSModuleNormalizeFunc*)r2qjs_normalize_module_name, r2qjs_load_module, NULL);
}

static int r2qjs_loader(JSContext *ctx, const char *const buffer) {
	JSRuntime *rt = JS_GetRuntime (ctx);
	if (!r_str_startswith (buffer, package_marker)) {
		return 0;
	}
	const char *ptr = buffer + strlen (package_marker);
	const char *ptr_end = buffer + strlen (buffer);
	const char *assets = strstr (ptr, delimiter_marker);
	if (!assets) {
		return -1;
	}

	HtPP *ht = ht_pp_new0 ();
	JS_SetModuleLoaderFunc (rt, (JSModuleNormalizeFunc*)r2qjs_normalize_module_name, r2qjs_load_module, ht);
	char *entry = NULL;

	assets += strlen (delimiter_marker);
	while (ptr < ptr_end && assets < ptr_end) {
		const char * nl = strchr (ptr, '\n');
		if (!nl) {
			break;
		}
		int size = atoi (ptr);
		if (size < 1) {
			break;
		}
		const char *const space = strchr (ptr, ' ');
		if (!space) {
			break;
		}
		char *filename = r_str_ndup (space + 1, nl - space - 1);
		char *data = r_str_ndup (assets, size);
		if (r_str_endswith (filename, ".js")) {
			// R_LOG_DEBUG ("File: (%s) Size: (%d)", filename, size);
			// R_LOG_DEBUG ("DATA: %s", data);
			ht_pp_insert (ht, filename, data);
			if (!entry) {
				entry = data;
			}
		}
		ptr = nl + 1;
		assets += size + strlen (delimiter_marker);
	}
	if (entry) {
		JSValue v = JS_Eval (ctx, entry, strlen (entry), "-", JS_EVAL_TYPE_GLOBAL | JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
		if (JS_IsException (v)) {
			JSValue e = JS_GetException (ctx);
			r2qjs_dump_obj (ctx, e);
		}
	}
	ht_pp_free (ht);
	// JS_SetModuleLoaderFunc (rt, NULL, NULL, NULL);
	JS_SetModuleLoaderFunc (rt, (JSModuleNormalizeFunc*)r2qjs_normalize_module_name, r2qjs_load_module, NULL);
	return true;
}
