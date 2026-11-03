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

/* {"ok":true,"result":"2.2.0"} — release the string with hrt_free(). */
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

/* Anti-androgen support: dose band, coverage, modelled level, next dose due.
 * {"events":[…],"bodyWeightKg":70,"nowH":…,"windowDays":28}
 * Each compound also says which mechanism it acts through and whether a fall
 * in testosterone is expected from it; spironolactone and bicalutamide block
 * the receptor and are not expected to lower it. */
HRT_API char *hrt_blockade(const char *request_json);

/* Conservative progress over a day, a month and a year.
 * {"events":[…],"labs":[…],"bodyWeightKg":70,"nowH":…,"e2Target":{…}}
 * Suppression is measured from labs and reported as a 95% lower bound;
 * effectiveness is the weaker of modelled time-in-range and logged coverage. */
HRT_API char *hrt_progress(const char *request_json);

/* Several regimens settled side by side.
 * {"arms":[{"label":"weekly","schedule":{"route":"injection","ester":"EV",
 *           "doseMG":4,"intervalH":168}}, …],
 *  "bodyWeightKg":70,"e2Target":{…},"tTarget":{…},"maxCycles":128}
 * Each arm is read after it settles, in its own analyte's units, and the
 * envelope names the steadiest, the highest and the cheapest per week.
 * Compounds the model does not curve come back with "verdict":"notModelled"
 * and no curve, so the dose can still be compared. */
HRT_API char *hrt_compare(const char *request_json);

/* The record with one dose changed, against the record as logged.
 * {"events":[…],"nowH":…,"analyte":"e2","horizonH":672,
 *  "scenarios":[{"kind":"skipNextDose"},{"kind":"delayNextDose","hours":12}]}
 * Kinds: baseline, skipNextDose, doubleNextDose, halveNextDose,
 * delayNextDose{hours}, stopFor{hours}. Nothing at or before nowH is changed;
 * every row is read over the same window and reported as a percentage delta. */
HRT_API char *hrt_what_if(const char *request_json);

/* Read a lab panel back rather than fitting it.
 * {"labs":[…],"nowH":…,"e2Target":{…},"tTarget":{…},"cadenceDays":90}
 * Each reading is compared against the range for its own analyte and marked
 * below, inRange, above or noTarget. Trends need two readings and 10% of
 * movement; "cadence" is fresh, dueSoon, overdue or none. */
HRT_API char *hrt_lab_review(const char *request_json);

/* Bust, underbust and the cup size they imply.
 * {"measurements":[{"timeH":…,"bustCm":91,"underbustCm":76}],
 *  "system":"eu","heightCm":170,"weightKg":63}
 * The difference between the two circumferences is the measurement; the letter
 * is a convention, so every system is returned (eu, cn, uk, us). Growth is the
 * slope of the difference, not of the bust. No volume is estimated. */
HRT_API char *hrt_body(const char *request_json);

/* Several calls in one crossing of the FFI boundary.
 * {"calls":[{"function":"simulate","input":{…}}, …]} */
HRT_API char *hrt_batch(const char *request_json);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HRT_CORE_H */
