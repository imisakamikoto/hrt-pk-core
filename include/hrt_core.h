/*
 * hrt_core.h — pharmacokinetic engine for estradiol and testosterone HRT.
 *
 * One convention for every entry point:
 *
 *     char *out = hrt_simulate("{\"events\":[],\"bodyWeightKg\":70,\"nowH\":0}");
 *     // out is UTF-8 JSON: {"ok":true,"result":…} on success, or
 *     //                   {"ok":false,"code":"badRequest","error":"…"}
 *     hrt_free(out);
 *
 * The returned pointer is owned by the library; release it with hrt_free().
 * Every function is thread-safe: calls share no mutable state. Nothing here
 * touches the network, the clock or the filesystem.
 *
 * ABI version 2. See docs/API.md for the request and response shapes.
 */

#ifndef HRT_CORE_H
#define HRT_CORE_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(HRT_CORE_STATIC)
#    define HRT_API
#  elif defined(HRT_CORE_BUILD)
#    define HRT_API __declspec(dllexport)
#  else
#    define HRT_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define HRT_API __attribute__((visibility("default")))
#else
#  define HRT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Library identity                                                          */
/* ------------------------------------------------------------------------- */

/* Wire format version. Refuse a library whose value is not the one you were
 * compiled against. */
HRT_API int32_t hrt_abi_version(void);

/* {"ok":true,"result":"2.0.0"} — release the string with hrt_free(). */
HRT_API char *hrt_version(void);

/* Every function, with the fields it accepts, as JSON. */
HRT_API char *hrt_capabilities(void);

/* Release a string returned by any function in this header. Null is ignored.
 * Never call it twice on the same pointer. */
HRT_API void hrt_free(char *ptr);

/* ------------------------------------------------------------------------- */
/* Curves and statistics                                                     */
/* ------------------------------------------------------------------------- */

/* Concentration curves for a set of doses.
 *
 * {"events":[…],"bodyWeightKg":70,"nowH":12345.5,
 *  "birthYear":1996,"params":{…},"clearanceMult":1.0,
 *  "startH":…,"endH":…,"stepH":…,"includeStats":true,
 *  "e2Target":{"low":100,"high":200},"tTarget":{"low":400,"high":700}}
 *
 * result: {"timeH":[…],"e2PgMl":[…],"cpaNgMl":[…],"tNgDl":[…],"auc":…}
 * or null when there is nothing to simulate. With "includeStats" the envelope
 * also carries a "stats" member. */
HRT_API char *hrt_simulate(const char *request_json);

/* Peak, trough, mean, AUC and time-in-range.
 * Same fields as hrt_simulate; "startH"/"endH" select the window. */
HRT_API char *hrt_stats(const char *request_json);

/* Fit the model to lab results.
 * {"events":[…],"labs":[…],"bodyWeightKg":70,"nowH":…,
 *  "method":"mipd"|"ekf"|"ou_kalman"|"off",
 *  "historyMode":"retrospective"|"forward"} */
HRT_API char *hrt_calibrate(const char *request_json);

/* ------------------------------------------------------------------------- */
/* Planning                                                                  */
/* ------------------------------------------------------------------------- */

/* Where a repeated schedule settles.
 * {"schedule":{"route":"injection","ester":"EV","doseMG":4,"intervalH":168},
 *  "bodyWeightKg":70,"maxCycles":128,"includeSeries":false} */
HRT_API char *hrt_steady_state(const char *request_json);

/* Dose that places a trough, peak or mean inside a target range.
 * {"schedule":{…},"target":{"low":100,"high":200},"objective":"trough"} */
HRT_API char *hrt_plan_dose(const char *request_json);

/* Times the level falls through "low", shifted "leadH" earlier. */
HRT_API char *hrt_reminders(const char *request_json);

/* Coverage, on-time percentage and gaps against a fixed interval. */
HRT_API char *hrt_adherence(const char *request_json);

/* Supply runway at the planned rate.
 * {"onHandMG":30,"schedule":{…},"asOfH":…} */
HRT_API char *hrt_supply(const char *request_json);

/* ------------------------------------------------------------------------- */
/* Data and helpers                                                          */
/* ------------------------------------------------------------------------- */

/* Concentration units and ester dose equivalence. */
HRT_API char *hrt_convert(const char *request_json);

/* Esters, routes, units and default targets, as data. */
HRT_API char *hrt_catalog(const char *request_json);

/* Sublingual hold time <-> mucosal fraction. */
HRT_API char *hrt_sublingual(const char *request_json);

/* Which removal ended which patch application. */
HRT_API char *hrt_patches(const char *request_json);

/* Doses the model ignores, and curve/range conflicts. */
HRT_API char *hrt_safety(const char *request_json);

/* How far the curve moves when clearance, weight or dose is off by a percent. */
HRT_API char *hrt_sensitivity(const char *request_json);

/* Several calls in one crossing of the FFI boundary.
 * {"calls":[{"function":"simulate","input":{…}}, …]} */
HRT_API char *hrt_batch(const char *request_json);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HRT_CORE_H */
