Native OpenGL renderer imported for qualification, not a validated Disney gameplay backend.

Source: https://github.com/YYOzcan/Tobiichi-Port/tree/471c83180a8df575cff4abd3fae6c043b5096925/PS2Recomp/ps2xRuntime
This renderer credits Waffle565's God of War port and Taylor N. Albarnaz / LightVelox.
Preserved source copyright notices and GPL-3.0 license are included.
No PCSX2 GS implementation was imported.
Local changes: local include paths, existing GS interface compatibility, readback-only presentation.

Disney additions: bounded Windows GPU worker, deterministic ownership of queued uploads, FBA alpha-bit shader handling, explicit GPU completion in offline benchmarks.
