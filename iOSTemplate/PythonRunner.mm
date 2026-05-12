#import "PythonRunner.h"

#import <TargetConditionals.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <Python.h>

extern "C" PyObject *PyInit_pysf(void);
extern "C" PyObject *PyInit_EngineExt(void);
extern "C" PyObject *PyInit_GlobalExt(void);

static NSString *GetPlatformSiteSubdir(void) {
#if TARGET_OS_SIMULATOR
#if defined(__x86_64__)
	return @"iphonesimulator.x86_64";
#else
	return @"iphonesimulator.arm64";
#endif
#else
	return @"iphoneos.arm64";
#endif
}

static wchar_t *DecodeWideString(NSString *value) {
	return Py_DecodeLocale(value.UTF8String, NULL);
}

static void AppendSearchPath(PyConfig *config, NSString *path) {
	wchar_t *wide = DecodeWideString(path);
	if (!wide) {
		return;
	}
	PyWideStringList_Append(&config->module_search_paths, wide);
	PyMem_RawFree(wide);
}

static NSString *FetchAndFormatPythonException(void) {
	PyObject *type = NULL;
	PyObject *value = NULL;
	PyObject *traceback = NULL;
	PyErr_Fetch(&type, &value, &traceback);
	PyErr_NormalizeException(&type, &value, &traceback);

	PyObject *tracebackModule = PyImport_ImportModule("traceback");
	if (!tracebackModule) {
		PyErr_Clear();
		Py_XDECREF(type);
		Py_XDECREF(value);
		Py_XDECREF(traceback);
		return @"Python error (failed to import traceback)";
	}

	PyObject *formatException = PyObject_GetAttrString(tracebackModule, "format_exception");
	if (!formatException) {
		PyErr_Clear();
		Py_DECREF(tracebackModule);
		Py_XDECREF(type);
		Py_XDECREF(value);
		Py_XDECREF(traceback);
		return @"Python error (failed to access traceback.format_exception)";
	}

	PyObject *list = PyObject_CallFunctionObjArgs(formatException, type ? type : Py_None, value ? value : Py_None, traceback ? traceback : Py_None, NULL);
	if (!list) {
		PyErr_Clear();
		Py_DECREF(formatException);
		Py_DECREF(tracebackModule);
		Py_XDECREF(type);
		Py_XDECREF(value);
		Py_XDECREF(traceback);
		return @"Python error (failed to format exception)";
	}

	PyObject *empty = PyUnicode_FromString("");
	PyObject *joined = NULL;
	if (empty) {
		joined = PyUnicode_Join(empty, list);
	}

	NSString *result = @"Python error";
	if (joined) {
		const char *utf8 = PyUnicode_AsUTF8(joined);
		if (utf8) {
			result = [NSString stringWithUTF8String:utf8];
		}
	}

	Py_XDECREF(joined);
	Py_XDECREF(empty);
	Py_DECREF(list);
	Py_DECREF(formatException);
	Py_DECREF(tracebackModule);
	Py_XDECREF(type);
	Py_XDECREF(value);
	Py_XDECREF(traceback);
	return result;
}


@implementation PythonRunner

+ (NSString *)runEntryScript {
	NSString *resourcePath = [NSBundle mainBundle].resourcePath;
	NSString *stdlibPath = [resourcePath stringByAppendingPathComponent:@"python-stdlib"];
	NSString *libDynloadPath = [stdlibPath stringByAppendingPathComponent:@"lib-dynload"];
	NSString *platformSitePath = [[resourcePath stringByAppendingPathComponent:@"platform-site"] stringByAppendingPathComponent:GetPlatformSiteSubdir()];
	NSString *entryInBundle = [resourcePath stringByAppendingPathComponent:@"Entry.py"];

	NSFileManager *fileManager = [NSFileManager defaultManager];

	NSURL *appSupport = [fileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
	if (!appSupport) {
		return @"Failed to resolve Application Support directory";
	}

	NSURL *scriptsDir = [appSupport URLByAppendingPathComponent:@"scripts" isDirectory:YES];
	[fileManager createDirectoryAtURL:scriptsDir withIntermediateDirectories:YES attributes:nil error:nil];

	NSURL *entryOnDisk = [scriptsDir URLByAppendingPathComponent:@"Entry.py"];
	if ([fileManager fileExistsAtPath:entryOnDisk.path]) {
		[fileManager removeItemAtURL:entryOnDisk error:nil];
	}
	NSError *copyError = nil;
	if (![fileManager copyItemAtPath:entryInBundle toPath:entryOnDisk.path error:&copyError]) {
		return [NSString stringWithFormat:@"Failed to copy Entry.py: %@", copyError.localizedDescription];
	}

	NSString *scriptsInBundle = [resourcePath stringByAppendingPathComponent:@"scripts"];
	BOOL scriptsIsDir = NO;
	if ([fileManager fileExistsAtPath:scriptsInBundle isDirectory:&scriptsIsDir] && scriptsIsDir) {
		if ([fileManager fileExistsAtPath:scriptsDir.path]) {
			[fileManager removeItemAtURL:scriptsDir error:nil];
		}
		NSError *scriptsCopyError = nil;
		if (![fileManager copyItemAtPath:scriptsInBundle toPath:scriptsDir.path error:&scriptsCopyError]) {
			return [NSString stringWithFormat:@"Failed to copy scripts: %@", scriptsCopyError.localizedDescription];
		}
	}

	if (chdir(entryOnDisk.URLByDeletingLastPathComponent.path.fileSystemRepresentation) != 0) {
		return [NSString stringWithFormat:@"Failed to chdir to scripts directory (%d)", errno];
	}

	char cwdBuffer[PATH_MAX];
	const char *cwd = getcwd(cwdBuffer, sizeof(cwdBuffer));
	NSLog(@"[__PROJECT_NAME__] Running Entry.py. cwd=%s script=%@", cwd ? cwd : "(unknown)", entryOnDisk.path);

	PyImport_AppendInittab("pysf",      &PyInit_pysf);
	PyImport_AppendInittab("EngineExt", &PyInit_EngineExt);
	PyImport_AppendInittab("GlobalExt", &PyInit_GlobalExt);

	PyStatus status;
	PyConfig config;
	PyConfig_InitPythonConfig(&config);
	config.isolated = 1;
	config.use_environment = 0;
	config.site_import = 0;
	config.install_signal_handlers = 0;
	config.write_bytecode = 0;

	status = PyConfig_SetBytesString(&config, &config.program_name, "__PROJECT_NAME__");
	if (PyStatus_Exception(status)) {
		PyConfig_Clear(&config);
		return @"Failed to set Python program name";
	}

	status = PyConfig_SetBytesString(&config, &config.home, resourcePath.UTF8String);
	if (PyStatus_Exception(status)) {
		PyConfig_Clear(&config);
		return @"Failed to set Python home";
	}

	config.module_search_paths_set = 1;
	AppendSearchPath(&config, stdlibPath);
	AppendSearchPath(&config, libDynloadPath);
	AppendSearchPath(&config, platformSitePath);
	AppendSearchPath(&config, entryOnDisk.URLByDeletingLastPathComponent.path);

	status = Py_InitializeFromConfig(&config);
	PyConfig_Clear(&config);
	if (PyStatus_Exception(status)) {
		return @"Py_InitializeFromConfig failed";
	}

	NSLog(@"[__PROJECT_NAME__] Setting up Python I/O redirect...");
	if (PyRun_SimpleString(
		"import sys, os\n"
		"class _NSLogWriter:\n"
		"    def __init__(self):\n"
		"        self._buf = ''\n"
		"    def write(self, s):\n"
		"        if not s:\n"
		"            return\n"
		"        self._buf += s\n"
		"        while '\\n' in self._buf:\n"
		"            line, self._buf = self._buf.split('\\n', 1)\n"
		"            msg = '[Python] ' + line + '\\n'\n"
		"            os.write(2, msg.encode('utf-8', errors='replace'))\n"
		"    def flush(self):\n"
		"        if self._buf:\n"
		"            msg = '[Python] ' + self._buf + '\\n'\n"
		"            os.write(2, msg.encode('utf-8', errors='replace'))\n"
		"            self._buf = ''\n"
		"sys.stdout = sys.stderr = _NSLogWriter()\n"
		"sys.stderr.write('[PythonRunner] I/O redirect active\\n')\n"
	) != 0) {
		NSString *error = FetchAndFormatPythonException();
		Py_Finalize();
		NSLog(@"[__PROJECT_NAME__] I/O redirect setup failed: %@", error);
		return error;
	}
	NSLog(@"[__PROJECT_NAME__] I/O redirect ready. Registering C extensions...");

	if (PyRun_SimpleString(
		"import EngineExt, GlobalExt, sys\n"
		"sys.modules['Engine.EngineExt'] = EngineExt\n"
		"sys.modules['Global.GlobalExt'] = GlobalExt\n"
		"sys.stderr.write('[PythonRunner] EngineExt + GlobalExt registered\\n')\n"
	) != 0) {
		NSString *error = FetchAndFormatPythonException();
		Py_Finalize();
		NSLog(@"[__PROJECT_NAME__] Failed to register C extensions: %@", error);
		return error;
	}
	NSLog(@"[__PROJECT_NAME__] C extensions registered. Opening Entry.py...");

	FILE *fp = fopen(entryOnDisk.path.fileSystemRepresentation, "r");
	if (!fp) {
		Py_Finalize();
		NSLog(@"[__PROJECT_NAME__] Failed to open Entry.py (%d)", errno);
		return [NSString stringWithFormat:@"Failed to open Entry.py (%d)", errno];
	}

	NSLog(@"[__PROJECT_NAME__] Running Entry.py via PyRun_SimpleFileEx...");
	int rc = PyRun_SimpleFileEx(fp, entryOnDisk.path.fileSystemRepresentation, 1);
	if (rc != 0) {
		NSString *formatted = FetchAndFormatPythonException();
		Py_Finalize();
		NSLog(@"[__PROJECT_NAME__] Python error:\n%@", formatted);
		return formatted;
	}

	NSLog(@"[__PROJECT_NAME__] Entry.py finished.");
	Py_Finalize();
	return @"(script finished)\n";
}

@end
