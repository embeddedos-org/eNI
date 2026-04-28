// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

fn main() {
    let eni_root = std::env::var("ENI_ROOT")
        .unwrap_or_else(|_| "../../".to_string());

    let common_src = format!("{}/common/src", eni_root);
    let common_inc = format!("{}/common/include", eni_root);
    let platform_inc = format!("{}/platform/include", eni_root);

    cc::Build::new()
        .include(&common_inc)
        .include(&platform_inc)
        .file(format!("{}/types.c", common_src))
        .file(format!("{}/config.c", common_src))
        .file(format!("{}/event.c", common_src))
        .file(format!("{}/session.c", common_src))
        .file(format!("{}/dsp.c", common_src))
        .file(format!("{}/edf.c", common_src))
        .file(format!("{}/xdf.c", common_src))
        .file(format!("{}/log.c", common_src))
        .warnings(false)
        .compile("eni");

    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed={}", common_src);
}
