---
paths: "src/**/*.S"
---

# Assembly rules

Full rules: `docs/CODING_STANDARDS.md`.

- Follow AAPCS64: x0-x7 arguments, x19-x28 callee-saved, x29 frame pointer, x30 link register, never x18.
- Keep SP 16-byte aligned at every public boundary.
- Write assembly only for what C cannot do: reset entry, vectors, context save and restore, special instructions.
- Comment every system register write with the field and the reason.
- Add `isb` after a system register write that must take effect before the next instruction, and say so in the comment.
- From M7 on, every indirect branch target needs a `bti` landing pad.
