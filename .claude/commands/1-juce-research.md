# JUCE Plug‑in Adaptive Research Prompt

### TASK

1. **Scope Assessment**

   - Read **$ARGUMENT** and classify the plug‑in idea as  
     **standard** (well‑trodden: EQ, subtractive synth, simple delay)  
     **advanced** (shimmer reverb, multiband comp, wavetable synth)  
     **novel** (ML stem splitter, real‑time AI noise reduction).
   - Decide the optimal mix of open‑source repos, tutorials, and papers.

2. **Search & Compile Sources**

   - For _standard_: prioritise tutorials + proven repos; papers optional.
   - For _advanced_: balanced mix; include ≥ 3 recent papers.
   - For _novel_: prioritise peer‑reviewed papers & cutting‑edge repos; tutorials optional.
   - Apply relevance ≥ medium, skip duplicates, stop when new queries add no value.

3. **Generate output files**  
   Create/overwrite **research/** with four Markdown files:  
   `open_source.md`, `tutorials.md`, `papers.md`, `summary.md` (YAML where specified).

### MINIMUM COUNTS (by scope)

| scope    | repos | tutorials | papers |
| -------- | ----- | --------- | ------ |
| standard | ≥ 5   | ≥ 5       | 0‑2    |
| advanced | ≥ 8   | ≥ 5       | ≥ 3    |
| novel    | ≥ 6   | ≥ 3       | ≥ 5    |

Claude may exceed these if high‑quality items are plentiful; skip categories that provide zero value.

• For ideas tagged _standard_ or _advanced_, you may run the **Quick-Research** mode:  
 limit to the top 3 repos + 2 papers and skip tutorials/YAML explosion.

---

## 1. research/open_source.md

YAML keys: `name, url, last_commit (YYYY‑MM‑DD), license, build_status, highlight, known_issues`  
Note any repo that fails to build and how to fix. Order by relevance. For each open-source repo, record its SPDX license; skip if license
is unknown or more restrictive than LGPL unless Lex approves.

## 2. research/tutorials.md

YAML keys: `title, url, format (article|video|forum|blog_post), key_takeaway, relevance`

## 3. research/papers.md

YAML keys: `title, url, year, venue, core_insight, relevance`

## 4. research/summary.md (≤ 700 words, plain English)

0. **Research Plan** – scope classification and rationale for chosen source mix.
1. **Recurring Patterns**
2. **Best Practices & Common Pitfalls**
3. **Top 5 Repos** (ranked; reference `name`)
4. **Top 3 Papers** (ranked; reference `title`)
5. **Key Open Questions**

---

### GLOBAL RULES

• Use valid YAML lists in the first three files—two‑space indents, full URLs.  
• Cite pre‑2015 sources only if still state‑of‑the‑art.  
• **No extra stdout** beyond these four files.  
• Ensure each YAML list passes `yamllint`.
