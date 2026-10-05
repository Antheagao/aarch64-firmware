---
paths: "**/*.md"
---

# Markdown rules

- Put each full sentence on its own line.
  Keep normal Markdown structure: headings, lists, tables, and code blocks stay as they are.
- Never use the em dash character.
  Use a plain dash, a colon, or split the sentence.
- Use clear, simple wording.
  Prefer short sentences, active voice, and concrete names of registers, files, and commands.
- Put register, instruction, file, and command names in backticks: `SCR_EL3`, `eret`, `boot.S`, `make test`.
- Cite the source for architectural facts: the Arm ARM (DDI 0487), GIC spec (IHI 0069), PSCI (DEN 0022), or SMCCC (DEN 0028).
- Keep tables for comparisons and status; keep prose for reasoning.
