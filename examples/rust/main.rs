//! A tour of the Rust API: simulate, summarise, plan, and check the record.
//!
//!     cargo run --release --example demo

use std::collections::HashMap;

use hrt_core::model::{DoseEvent, Ester, PkParams, Route, TargetRange};
use hrt_core::{analysis, planning, reminders, safety, stats};

fn main() {
    let params = PkParams::default();
    let weight = 70.0;
    let range = TargetRange::new(100.0, 200.0);

    // Two months of weekly injections of 4 mg estradiol valerate.
    let events: Vec<DoseEvent> = (0..9)
        .map(|i| DoseEvent {
            id: format!("dose-{i}"),
            route: Route::Injection,
            time_h: i as f64 * 168.0,
            dose_mg: 4.0,
            ester: Ester::EV,
            extras: HashMap::new(),
            updated_at: None,
        })
        .collect();

    let now = 8.0 * 168.0 + 24.0;
    let sim = hrt_core::run_simulation(&events, weight, None, &params, now, 1.0).expect("a curve");

    let summary = stats::simulation_stats(&sim, Some((7.0 * 168.0, now)), Some(range), None);
    if let Some(e2) = &summary.e2 {
        println!(
            "last week: peak {:.0} at {:.0} h, trough {:.0}, mean {:.0} pg/mL, {:.0}% in range",
            e2.peak, e2.peak_h, e2.trough, e2.mean, e2.time_in_range_pct
        );
    }

    // Where would this schedule settle if nothing changed?
    let schedule = planning::Schedule {
        route: Route::Injection,
        ester: Ester::EV,
        dose_mg: 4.0,
        interval_h: 168.0,
        extras: HashMap::new(),
        start_h: now,
    };
    if let Some(steady) = planning::steady_state(
        &schedule,
        weight,
        None,
        &params,
        128,
        Some(range),
        None,
        false,
    ) {
        if let Some(e2) = &steady.cycle.e2 {
            println!(
                "steady state after {} doses (stable: {}): peak {:.0}, trough {:.0}",
                steady.cycles, steady.stable, e2.peak, e2.trough
            );
        }
    }

    // What dose would put the trough at the bottom of the range?
    match planning::solve_dose(
        &schedule,
        weight,
        None,
        &params,
        planning::Objective::Trough,
        range,
        128,
    ) {
        Some(sol) => println!(
            "suggested {:.1} mg every {} h → trough {:.0}, peak {:.0}{}",
            sol.dose_mg,
            schedule.interval_h as i64,
            sol.achieved_trough,
            sol.achieved_peak,
            if sol.in_range { "" } else { " (out of range)" }
        ),
        None => println!("no solution for this schedule"),
    }

    // When would a reminder be useful?
    if let Some(drop) = reminders::next_drop(&sim, range.low, now) {
        println!(
            "next dip below {:.0} at {:.1} h ({:.1} h from now)",
            drop.level, drop.crossing_h, drop.hours_until
        );
    }

    // Does the record contain something the model silently ignores?
    let warnings = safety::check(safety::SafetyInput {
        events: &events,
        labs: &[],
        params: &params,
        stats: Some(&summary),
        e2_range: Some(range),
        t_range: None,
        weight,
        now_h: now,
    });
    println!("{} warning(s):", warnings.len());
    for warning in &warnings {
        println!(
            "  [{}] {} — {}",
            warning.severity, warning.code, warning.message
        );
    }

    // How much does a 20% clearance error matter?
    for point in analysis::sensitivity(
        &events,
        weight,
        None,
        &params,
        now,
        1.0,
        "clearance",
        20.0,
        Some((7.0 * 168.0, now)),
        Some(range),
        None,
    ) {
        println!(
            "clearance {:+.0}% → trough {:+.1}%",
            point.delta_pct, point.trough_change_pct
        );
    }
}
