/*
 * A minimal C client: simulate a schedule, summarise it, and ask what dose
 * would land in a target range.
 *
 *   cc -Iinclude examples/c/example.c -Ltarget/release -lhrt_core \
 *      -Wl,-rpath,$PWD/target/release -o /tmp/hrt_example
 *   /tmp/hrt_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hrt_core.h"

/* Every call returns a string that has to be freed, including failures. */
static char *must_call(char *(*fn)(const char *), const char *json) {
    char *out = fn(json);
    if (out == NULL) {
        fprintf(stderr, "call returned NULL\n");
        exit(1);
    }
    return out;
}

int main(void) {
    char *version = hrt_version();
    if (version == NULL) return 1;
    printf("core: %s (abi %d)\n", version, hrt_abi_version());
    hrt_free(version);

    /* One 4 mg estradiol valerate injection, read 72 hours later. */
    const char *sim_request =
        "{\"events\":[{\"id\":\"a\",\"route\":\"injection\",\"timeH\":0,"
        "\"doseMG\":4,\"ester\":\"EV\"}],"
        "\"bodyWeightKg\":70,\"nowH\":72,\"includeStats\":true,"
        "\"e2Target\":{\"low\":100,\"high\":200}}";
    char *sim = must_call(hrt_simulate, sim_request);
    if (strstr(sim, "\"ok\":true") == NULL) {
        fprintf(stderr, "simulate failed: %s\n", sim);
        hrt_free(sim);
        return 1;
    }
    /* The curve is large; this example only shows that it is there and that
     * the summary came back in the same reply. */
    printf("simulate: %zu bytes, stats %s\n", strlen(sim),
           strstr(sim, "\"stats\"") ? "included" : "missing");
    hrt_free(sim);

    /* What weekly dose puts the trough at the bottom of 100–200 pg/mL? */
    const char *plan_request =
        "{\"schedule\":{\"route\":\"injection\",\"ester\":\"EV\","
        "\"doseMG\":4,\"intervalH\":168},"
        "\"target\":{\"low\":100,\"high\":200},\"objective\":\"trough\","
        "\"bodyWeightKg\":70}";
    char *plan = must_call(hrt_plan_dose, plan_request);
    printf("plan: %s\n", plan);
    hrt_free(plan);

    /* Unit conversion needs no simulation at all. */
    char *conv = must_call(
        hrt_convert,
        "{\"kind\":\"concentration\",\"value\":150,\"from\":\"pg/ml\","
        "\"to\":\"pmol/l\",\"analyte\":\"e2\"}");
    printf("convert: %s\n", conv);
    hrt_free(conv);

    return 0;
}
