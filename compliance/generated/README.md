# Generated Fixtures

This directory holds committed C fixtures translated from real JavaScript compliance tests.

Guidelines:

- Keep outputs reviewable and self-contained.
- Include `compliance/generated/test_contract.h`.
- Preserve the upstream test intent in a short comment near the top of each file.
- Do not regenerate fixtures in `make` or CI. Translation is an authoring step.

Suite-specific outputs live under subdirectories such as `strings/`.
