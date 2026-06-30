# Doodle Dungeon — Deep Design & Market Analysis

> Companion to `PHONE_GAME_CONCEPT.md`. This document does the homework before any
> code: it researches the competition, sharpens the wedge, specifies the full
> drawing-symbol language, lays out a concrete game-design spec, and picks a
> monetization model — all grounded in current market data (sources at the end).

---

## 1. The sharpened wedge (read this first)

The original pitch — "draw on paper, snap a photo, play your drawing" — is **not
novel**. At least three shipping products already do exactly that loop. **But every
one of them produces a 2D game** (platformer / arcade / physics). Separately, "turn
a floor plan into 3D" is a *solved* problem — but only in architecture/real-estate
tools that produce **static visualizations, not games**.

> **The white space nobody occupies: a hand-drawn *floor plan* → a *3D place you
> explore and play in*.** That intersection is precisely what a 3D engine like
> IKore is built for, and it is invisible to both the 2D "draw-a-game" apps and the
> non-game floor-plan tools.

Reframe the product accordingly: **you are not drawing a *game*, you are drawing a
*place*** — a dungeon, a house, a spaceship — and then living inside it in 3D.
That single word ("place," not "game") is the whole differentiation.

A lucky bonus: **floor plans literally *are* dungeon maps.** "Rooms connected by
corridors" is the native grammar of the entire dungeon-crawler genre, so the
spatial structure of a floor plan maps onto a fun game with almost no translation.

---

## 2. Market & competitor research

### 2.1 Category A — "draw your own game" apps (direct loop, but 2D)

| Product | What it does | Model | Limit / opening |
|---|---|---|---|
| **Pixicade** (Abacus Brands) | Draw on paper → photo → 2D game. Templates: platformer, **maze**, slingshot, brick-breaker, 2-player. Sold as a physical marker **kit** for kids 6–12; award-winning (Newsweek STEM, Mom's Choice) | Physical kit + app, IAP ("snowflakes") | **2D only**; template-bound; some reviews cite confusing premium currency |
| **Draw Your Game Infinite** (Korrisoft) | Draw on paper → photo → 2D physics platformer. **Color = behavior**: black static, red danger, green bouncy, blue movable | Free-to-play + IAP | **2D only**; fixed physics-platformer genre |
| **DoodleMatic** | Draw → photo → playable 2D game; community sharing | App | **2D only** |

**Key takeaways:**
- The "snap a drawing and play it" loop is validated and *liked* — but it has
  calcified around **2D arcade/physics**.
- **Color-coding beats shape recognition.** Draw Your Game's 4-color language is a
  deliberate CV-robustness choice — segmenting by color is far more reliable on
  messy drawings than classifying hand-drawn shapes. **We should adopt this.**
- **The physical kit (Pixicade) is both a revenue channel and a CV hack** —
  controlled marker colors and paper make recognition dramatically easier.

### 2.2 Category B — floor plan → 3D (solved, but not games)

Polycam (LiDAR room scan), magicplan (AR corner detection), Planner 5D (AI room
scan), floor-plan.ai and Coohom (**upload a photo of a hand-drawn sketch → 3D via
auto-wall-detection**). These prove the conversion is **feasible today** and give
us prior art to learn from — but they output **furniture-planning visualizations**,
not anything playable (no goals, enemies, movement, scoring, or sharing).

### 2.3 Category C — sketch → 3D *asset* AI (adjacent)

Meshy, Sloyd.ai ("drawing to dungeon"), Alpha3D, Adobe **Aqua** (kid-friendly 2D→3D
with AR). These make 3D **objects/models**, not navigable **spaces**. Useful later
as a way to turn doodled *props* into meshes; not a competitor to the core loop.

### 2.4 Market validation for the model

- **UGC + sharing is a proven engine.** Super Mario Maker had **7M+ user courses
  played 600M+ times by 2016**, with a simple "stars" approval mechanic driving
  organic, word-of-mouth marketing. Our share-a-map loop targets the same dynamic.
- **Kids creative-STEM has real commercial pull** — Pixicade's awards and retail
  presence show parents pay for "creativity → play" products.

### 2.5 The competitive map (where we sit)

```
            2D / arcade            3D / spatial
          ┌──────────────────┬──────────────────────┐
 GAME     │ Pixicade,        │   ★ DOODLE DUNGEON ★  │  ← empty quadrant
 (play)   │ Draw Your Game,  │   (our wedge)         │
          │ DoodleMatic      │                       │
          ├──────────────────┼──────────────────────┤
 NOT a    │ (n/a)            │ Polycam, magicplan,   │
 game     │                  │ floor-plan.ai, Coohom │
          └──────────────────┴──────────────────────┘
```

**Primary threat:** an incumbent (esp. Pixicade, which already has a "Maze Maker")
adds a 3D mode. **Our moat against that:** a 3D engine purpose-built for
world-from-data, a floor-plan-native symbol language, and a UGC community — i.e.
depth they'd have to rebuild. Speed and focus matter.

---

## 3. Positioning & audience

Two viable audiences; they imply different art, tone, and **monetization**:

| | **Kids 6–12 (creative/STEM)** | **Teens/Adults (UGC dungeon-makers)** |
|---|---|---|
| Hook | "Your drawing comes to life in 3D" | "Sketch a dungeon, share it, top the leaderboard" |
| Tone/art | Bright, friendly, safe | Stylized dungeon/roguelite |
| Distribution | Apple **Kids Category**, retail kit | Standard F2P, social/UGC virality |
| Monetization | **Premium / kit / parent-gated** (ads largely banned — see §6) | **Hybrid F2P** (ads + IAP) viable |
| Compliance | Heavy (COPPA/GDPR-K) | Standard |

**Recommendation:** launch for **teens/adults first** (the "dungeon-maker" framing).
Reasons: monetization is far less constrained (§6), UGC virality is stronger in that
demographic, and the dungeon aesthetic makes "floor plan = level" feel intentional
rather than mundane. Add a **Kids Mode + physical kit** as a second act once the
core loop and CV are proven — that's where Pixicade's playbook (and revenue) lives,
but it carries the compliance burden.

---

## 4. The symbol language (full specification)

### 4.1 Design principles
1. **A child can draw every symbol** with a single pen, first try.
2. **Robust to CV before clever.** Prefer features that survive a blurry photo.
3. **Unambiguous.** No two symbols collide after line-simplification.
4. **Progressive.** Start with ~5 symbols; unlock more as the player levels up — this
   doubles as onboarding (you can't be overwhelmed by a vocabulary you haven't
   unlocked yet).

### 4.2 The hybrid scheme: **color = class, shape = subtype**
Adopt Draw Your Game's color insight, but extend it for 3D. **Color carries the
robust, must-not-fail signal** (is this a wall? an enemy? loot?); **shape refines it**
when recognition is confident, and degrades gracefully to a sensible default when
it isn't.

| Color | Class | Why |
|---|---|---|
| **Black** | Structure (walls, doors) | The bulk of every drawing; highest-contrast |
| **Red** | Threats (enemies, hazards) | Universally reads as "danger" |
| **Green** | Goals & collectibles | Reads as "good/go" |
| **Blue** | Interactive (doors that open, switches, keys, water) | Distinct, cool-toned |
| **(pencil/uncolored)** | Auto-detected walls | Lets pure-pencil drawings still work |

### 4.3 The vocabulary

**Tier 1 — taught in the first 60 seconds (everyone starts here):**

| Draw | Color | Becomes |
|---|---|---|
| Lines / closed shapes | Black | Walls + rooms (extruded to 3D) |
| Gap in a wall | — | Doorway / passage |
| `S` or a star | Green | Player start |
| `X` or a flag | Green | Exit / goal |
| Small filled dot | Green | Coin / collectible |
| Spiky blob | Red | Basic enemy spawn |

**Tier 2 — unlocked by playing:**

| Draw | Color | Becomes |
|---|---|---|
| Wavy / zigzag fill | Red | Hazard floor (lava, spikes) |
| Double line / "=" across a gap | Blue | Door that opens (with a key) |
| Small key shape | Blue | Key (opens matching door) |
| Triangle cluster | Red | Ranged/turret enemy |
| `+` | Green | Health pickup |
| Spiral | Blue | Teleporter (pairs nearest two) |

**Tier 3 — advanced authoring:**

| Draw | Color | Becomes |
|---|---|---|
| Number `1`–`3` in a room | Black | **Floor height / elevation** (verticality — multi-storey!) |
| Hatched region | Black | Stairs/ramp between heights |
| Circle with a face | Blue | NPC (gives a line of text / quest) |
| Box outline | Blue | Pushable crate (physics) |
| `~` region | Blue | Water (swim/slow) |

> **Verticality (Tier 3) is a stealth differentiator.** Because IKore is true 3D,
> writing a small "2" in a room can raise it a storey — something *no* 2D
> competitor can express. A flat floor plan becomes a multi-level space.

### 4.4 Drawing rules & CV-robustness aids
- **Guide paper.** Offer a free printable (and the §6 physical kit) with a faint grid
  and a color legend — massively improves recognition, like Pixicade's kit.
- **Snap-to-grid + angle-snapping** turns wobbly lines into clean architecture.
- **Confidence + repair UI.** If a symbol is ambiguous, the app shows its
  interpretation as editable icons *before* play ("we found 3 enemies and 2 coins —
  tap to fix"). Never fail silently; turn correction into part of the fun.
- **Graceful defaults.** Unknown red mark → basic enemy; unknown green → coin;
  unclosed black region → best-effort wall with a highlighted warning.

---

## 5. Game-design spec

### 5.1 Core fantasy
> "I designed this place, and now I'm *inside* it — and so is everyone I share it with."

### 5.2 Core loops
- **Micro (seconds–minutes):** Draw → scan → **review/fix symbols** → play → tweak the
  paper → re-scan. Fast iteration is the addiction.
- **Macro (sessions):** Play → earn XP → **unlock new symbols/themes/characters** →
  author more ambitious maps → **publish & get stars** → climb leaderboards.

### 5.3 Primary mode: 3D dungeon crawl (ship this first)
- **Camera:** over-the-shoulder / adjustable top-down-3D. Reads clearly regardless of
  drawing quality and shows off the 3D wow without demanding twitch precision.
- **Verbs:** move, dodge, attack/interact, collect, reach the exit.
- **Why first:** easiest to make *fun* on an arbitrary floor plan; "rooms + corridors
  + enemies + loot + exit" is a complete game with the Tier-1 vocabulary alone.

### 5.4 Secondary modes (post-launch, same maps)
First-person **Tour mode** (the money shot for share clips) · **Stealth/escape** vs.
`AIComponent` agents · **Tower defense** (enemies path your corridors) · **Race/
time-attack** (leaderboard fuel) · **Co-op/async multiplayer** via share codes.

### 5.5 Difficulty — *derived from the drawing*
No difficulty slider; the map *is* the difficulty. Derive a star rating from
enemy density, path length, hazard coverage, and key/lock depth, and surface it so
authors can self-balance ("this map is rated ★★★★☆ Brutal").

### 5.6 Meta-progression & retention
- **XP & symbol unlocks** (Tier 1→3) give a reason to keep playing and authoring.
- **Themes/skins** (Dungeon, Haunted House, Spaceship, Neon City) reskin the *same*
  geometry — cheap content, strong personalization, natural IAP.
- **UGC + "stars"** (proven by Mario Maker): publish maps, browse by Hot/New/Top, award
  stars, follow creators.
- **Weekly drawing prompts/challenges** ("draw a castle") → curated feed → fresh
  content engine without us building levels.
- **Share code + side-by-side clip** (paper ↔ 3D) as the built-in virality hook.

### 5.7 Onboarding
First run ships a **pre-printed trace sheet**: the player traces a tiny 3-room map,
scans it, and is playing in <90 seconds — guaranteeing the magic moment lands before
they face a blank page.

### 5.8 The biggest design risk (state it plainly)
**A real floor plan is not automatically a fun level.** Mitigations: (a) the symbol
language injects game content (enemies/loot/goals/keys) onto any space; (b) the
converter *embellishes* — themed lighting, props, verticality; (c) the **dungeon
framing** makes rooms-and-corridors a feature, not a bug; (d) the difficulty
rating + fix-up UI nudge authors toward playable layouts. **This must be validated
in the desktop PoC before any mobile spend.**

---

## 6. Monetization

### 6.1 Benchmarks (2025–2026)
- **Hybrid-casual ARPDAU ≈ $0.15–0.50**, *4–7× higher than hypercasual* ($0.03–0.08);
  casual target ≈ $0.80; Party/Match genres reach the highest ARPU ($4.90 / $2.99).
- **Rewarded video = ~62% of mobile-game ad revenue**, with 2–3× the engagement of
  interstitials — the *only* ad format players tolerate.
- Hybrid-casual typically runs a **~45/55 IAP/ads split**; hybrid IAP grew ~100% YoY
  (>$345M H1 2025).

### 6.2 The kids constraint changes everything
If we target the **Kids** audience/category, **ads are effectively off the table**:
Apple's Kids Category forbids third-party ad SDKs and analytics and requires parental
gates; the 2025 COPPA rule (in force April 22, 2026) makes biometric data PII and
demands separate consent to share kids' data with advertisers; **AppLovin stopped
serving ads to COPPA users in Jan 2025**. Best practice for kids' apps is to **avoid
in-game ads entirely** and monetize via premium/subscription/parent-gated IAP.

### 6.3 Recommended model — by audience

**Teens/adults (primary launch):** **hybrid free-to-play.**
- **Rewarded video only** (never forced interstitials mid-creation): optional "watch
  to unlock a theme for a day," "revive," "extra map slot."
- **IAP, cosmetic & convenience — never pay-to-win:** theme/skin packs, character
  skins, symbol-style packs, extra cloud map slots, "remove ads."
- **Optional subscription** ("Architect's Pass"): all themes, unlimited cloud maps,
  early symbols, creator analytics.

**Kids (second act):** **premium + physical kit, no ads.**
- Paid app or a parent-gated one-time unlock; **Pixicade-style physical marker kit**
  as the hero retail SKU (doubles as the CV-robustness aid from §4.4); optional
  family subscription for cloud storage. All purchases behind a parental gate.

### 6.4 Guardrails (non-negotiable)
No pay-to-win, no manipulative currencies, no ads in the kids build, IAP always
skippable and clearly labeled, COPPA/GDPR-K compliance designed in from day one.

---

## 7. Risks & open questions

| Risk | Severity | Mitigation |
|---|---|---|
| "Is exploring a floor plan actually fun?" | **High** | Validate in desktop PoC first (§5.8) |
| CV robustness on real drawings | **High** | Color-coding (§4.2), guide paper/kit, confidence+repair UI, Phase 0→2 ramp |
| Incumbent (Pixicade) adds 3D | Medium | Move fast; depth via 3D engine + verticality + UGC moat |
| Mobile port cost (IKore is desktop) | Medium | Prove on desktop; keep converter renderer-agnostic (see concept doc) |
| Kids-compliance overhead | Medium | Launch adult-first; add kids mode deliberately later |

---

## 8. What this means for the build (unchanged first step)

None of this alters the first technical move from `PHONE_GAME_CONCEPT.md`: build the
**renderer-agnostic converter core** and prove the loop on desktop. This research
only *constrains the design* of that core:
- Input model = **color-class + shape-subtype** (§4.2), not shape-only.
- Output = a **3D, optionally multi-storey** scene (verticality from Tier-3 symbols).
- Success metric for the PoC = **"is the resulting 3D space fun to move through?"**
  (§5.8) — the make-or-break question, answered cheaply before any mobile spend.

---

## 9. Sources

- Draw-a-game apps: [Draw Your Game Infinite (Google Play)](https://play.google.com/store/apps/details?id=com.korrisoft.draw.your.game), [Pixicade (App Store)](https://apps.apple.com/us/app/pixicade-game-creator/id1532289517), [Pixicade (Abacus Brands)](https://www.abacusbrands.com/products/pixicade), [DoodleMatic review](https://btr.michaelkwan.com/2020/12/09/doodlematic-review/), [Pixicade review (GeekDad)](https://geekdad.com/2021/02/games-from-pixicade-are-as-unlimited-as-your-imagination/)
- Floor plan → 3D (incl. hand-drawn): [floor-plan.ai](https://floor-plan.ai/floor-plan-to-3d), [Coohom](https://www.coohom.com/article/convert-2d-floor-plan-to-3d-model-free-online), [Polycam](https://poly.cam/floor-plans), [magicplan](https://magicplan.app/blog/room-scan-cad), [Planner 5D](https://planner5d.com/ai)
- Sketch → 3D assets: [Sloyd.ai "drawing to dungeon"](https://www.sloyd.ai/blog/from-drawing-to-dungeon-turn-fantasy-sketches-into-3d-maps), [Meshy sketch-to-3D](https://www.meshy.ai/blog/sketch-to-3d), [Adobe Aqua](https://aqua.adobe.com/learn/3d-drawing-apps)
- UGC market validation: [Super Mario Maker (Wikipedia)](https://en.wikipedia.org/wiki/Super_Mario_Maker), [Why Mario Maker is the perfect UGC tool (Lurkit)](https://www.lurkit.gg/blog/why-nintendos-mario-maker-is-the-perfect-ugc-marketing-tool)
- Monetization benchmarks: [Hybrid casual 2026 guide (Game Growth Advisor)](https://gamegrowthadvisor.com/blog/2026-04-16-hybrid-casual-game-design-strategy-2026/), [ARPDAU by genre (Juego)](https://www.juegostudio.com/blog/arpdau-benchmarks-by-game-genre), [Tenjin Ad Monetization 2026](https://tenjin.com/blog/ad-mon-gaming-2026/)
- Kids monetization & compliance: [Monetization for kids apps & COPPA (Openback)](https://openback.com/blog/monetization-for-kids-apps-how-to-be-profitable-and-coppa-compliant/), [Ad monetization in kids games (GameBiz)](https://www.gamebizconsulting.com/blog/ad-monetization-in-kids-games-and-apps-how-to-do-it-right), [COPPA 2025 guide (Promise Legal)](https://blog.promise.legal/startup-central/coppa-compliance-in-2025-a-practical-guide-for-tech-edtech-and-kids-apps/), [COPPA/GDPR-K for kids' games (Fish in a Bottle)](https://www.fishinabottle.com/blog/what-does-coppa-and-gdpr-k-compliance-mean-for-childrens-games-fish-in-a-bottle)
