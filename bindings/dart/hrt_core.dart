/// Dart binding for hrt_core.
///
/// Works on Android, iOS, macOS, Linux and Windows. Add `ffi: ^2.1.0` to
/// `pubspec.yaml`; nothing else is required.
///
/// ```dart
/// final core = HrtCore.open();
/// final curve = core.simulate({
///   'events': [
///     {'id': 'a', 'route': 'injection', 'timeH': 0, 'doseMG': 4, 'ester': 'EV'},
///   ],
///   'bodyWeightKg': 70,
///   'nowH': 72,
/// });
/// ```
///
/// The native call is synchronous and typically takes well under a millisecond,
/// so it is fine on the UI isolate for a single simulation; wrap longer calls
/// (steady state, calibration) in `Isolate.run` if the chart is animating.
library;

import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

/// The library rejected a request.
class HrtException implements Exception {
  HrtException(this.code, this.message);

  /// Machine-readable reason, e.g. `badRequest` or `unknownFunction`.
  final String code;
  final String message;

  @override
  String toString() => 'HrtException($code): $message';
}

typedef _JsonCallNative = Pointer<Utf8> Function(Pointer<Utf8>);
typedef _JsonCall = Pointer<Utf8> Function(Pointer<Utf8>);
typedef _NoArgCall = Pointer<Utf8> Function();
typedef _FreeNative = Void Function(Pointer<Utf8>);
typedef _AbiNative = Int32 Function();

class HrtCore {
  HrtCore._(this._lib) {
    _free = _lib.lookupFunction<_FreeNative, _FreeNative>('hrt_free');
    _abi = _lib.lookupFunction<_AbiNative, _AbiNative>('hrt_abi_version');
    _simulate = _lookup('hrt_simulate');
    _stats = _lookup('hrt_stats');
    _calibrate = _lookup('hrt_calibrate');
    _steady = _lookup('hrt_steady_state');
    _plan = _lookup('hrt_plan_dose');
    _reminders = _lookup('hrt_reminders');
    _adherence = _lookup('hrt_adherence');
    _supply = _lookup('hrt_supply');
    _convert = _lookup('hrt_convert');
    _catalog = _lookup('hrt_catalog');
    _sublingual = _lookup('hrt_sublingual');
    _patches = _lookup('hrt_patches');
    _safety = _lookup('hrt_safety');
    _sensitivity = _lookup('hrt_sensitivity');
    _batch = _lookup('hrt_batch');
  }

  /// Open the library that ships with the host app.
  ///
  /// On iOS the core is linked statically into the executable, so there is no
  /// separate image to open and `DynamicLibrary.process()` resolves the
  /// symbols from the app itself.
  factory HrtCore.open({String? path}) {
    final lib = path != null
        ? DynamicLibrary.open(path)
        : Platform.isIOS
        ? DynamicLibrary.process()
        : Platform.isAndroid
        ? DynamicLibrary.open('libhrt_core.so')
        : Platform.isWindows
        ? DynamicLibrary.open('hrt_core.dll')
        : Platform.isMacOS
        ? DynamicLibrary.open('libhrt_core.dylib')
        : DynamicLibrary.open('libhrt_core.so');
    return HrtCore._(lib);
  }

  final DynamicLibrary _lib;
  late final _FreeNative _free;
  late final _AbiNative _abi;
  late final _JsonCall _simulate;
  late final _JsonCall _stats;
  late final _JsonCall _calibrate;
  late final _JsonCall _steady;
  late final _JsonCall _plan;
  late final _JsonCall _reminders;
  late final _JsonCall _adherence;
  late final _JsonCall _supply;
  late final _JsonCall _convert;
  late final _JsonCall _catalog;
  late final _JsonCall _sublingual;
  late final _JsonCall _patches;
  late final _JsonCall _safety;
  late final _JsonCall _sensitivity;
  late final _JsonCall _batch;

  _JsonCall _lookup(String name) =>
      _lib.lookupFunction<_JsonCallNative, _JsonCall>(name);

  // --- identity ---------------------------------------------------------

  int get abiVersion => _abi();

  String get version {
    final noArg = _lib.lookupFunction<_NoArgCall, _NoArgCall>('hrt_version');
    final pointer = noArg();
    final text = pointer.toDartString();
    _free(pointer);
    final envelope = jsonDecode(text) as Map<String, dynamic>;
    return envelope['result'] as String;
  }

  Map<String, dynamic> capabilities() {
    final noArg = _lib.lookupFunction<_NoArgCall, _NoArgCall>('hrt_capabilities');
    final pointer = noArg();
    final text = pointer.toDartString();
    _free(pointer);
    final envelope = jsonDecode(text) as Map<String, dynamic>;
    return envelope['result'] as Map<String, dynamic>;
  }

  // --- plumbing ---------------------------------------------------------

  dynamic _call(_JsonCall fn, Map<String, Object?> request) {
    final payload = jsonEncode(request).toNativeUtf8();
    final Pointer<Utf8> pointer;
    try {
      pointer = fn(payload);
    } finally {
      malloc.free(payload);
    }
    if (pointer == nullptr) {
      throw HrtException('null', 'the library returned a null pointer');
    }
    try {
      final text = pointer.toDartString();
      final envelope = jsonDecode(text) as Map<String, dynamic>;
      if (envelope['ok'] != true) {
        throw HrtException(
          envelope['code'] as String? ?? '',
          envelope['error'] as String? ?? 'unknown error',
        );
      }
      return envelope['result'];
    } finally {
      _free(pointer);
    }
  }

  /// Any function by name, with an already-shaped request.
  Map<String, dynamic> invoke(String function, Map<String, Object?> request) {
    final fn = _lookup(function);
    final result = _call(fn, request);
    return result == null ? <String, dynamic>{} : result as Map<String, dynamic>;
  }

  // --- the API ----------------------------------------------------------

  Map<String, dynamic>? simulate(Map<String, Object?> request) =>
      _call(_simulate, request) as Map<String, dynamic>?;

  Map<String, dynamic>? stats(Map<String, Object?> request) =>
      _call(_stats, request) as Map<String, dynamic>?;

  Map<String, dynamic> calibrate(Map<String, Object?> request) =>
      _call(_calibrate, request) as Map<String, dynamic>;

  Map<String, dynamic>? steadyState(Map<String, Object?> request) =>
      _call(_steady, request) as Map<String, dynamic>?;

  Map<String, dynamic>? planDose(Map<String, Object?> request) =>
      _call(_plan, request) as Map<String, dynamic>?;

  Map<String, dynamic> reminders(Map<String, Object?> request) =>
      _call(_reminders, request) as Map<String, dynamic>;

  Map<String, dynamic>? adherence(Map<String, Object?> request) =>
      _call(_adherence, request) as Map<String, dynamic>?;

  Map<String, dynamic>? supply(Map<String, Object?> request) =>
      _call(_supply, request) as Map<String, dynamic>?;

  Map<String, dynamic> convert(Map<String, Object?> request) =>
      _call(_convert, request) as Map<String, dynamic>;

  Map<String, dynamic> catalog() => _call(_catalog, const {}) as Map<String, dynamic>;

  Map<String, dynamic> sublingual(Map<String, Object?> request) =>
      _call(_sublingual, request) as Map<String, dynamic>;

  List<dynamic> patches(List<Map<String, Object?>> events) =>
      _call(_patches, {'events': events}) as List<dynamic>;

  List<dynamic> safety(Map<String, Object?> request) =>
      _call(_safety, request) as List<dynamic>;

  List<dynamic> sensitivity(Map<String, Object?> request) =>
      _call(_sensitivity, request) as List<dynamic>;

  /// Several calls in one crossing of the FFI boundary. Failures are returned
  /// inline as envelopes rather than thrown, so one bad call cannot lose the
  /// rest of a batch.
  List<dynamic> batch(List<Map<String, Object?>> calls) {
    final result = _call(_batch, {'calls': calls}) as Map<String, dynamic>;
    return result['results'] as List<dynamic>;
  }
}
