#ifndef TRAIT_DEFS
#define TRAIT_DEFS

namespace fallout {

// The maximum number of traits a player is allowed to select.
// FO2 default is 2; FO1 allows 3. Array-bound sized for 3 (FO1 maximum).
// Use traitGetMaxSelectedCount() for runtime selection limit.
#define TRAITS_MAX_SELECTED_COUNT 3

// Available traits.
typedef enum Trait {
    TRAIT_FAST_METABOLISM,
    TRAIT_BRUISER,
    TRAIT_SMALL_FRAME,
    TRAIT_ONE_HANDER,
    TRAIT_FINESSE,
    TRAIT_KAMIKAZE,
    TRAIT_HEAVY_HANDED,
    TRAIT_FAST_SHOT,
    TRAIT_BLOODY_MESS,
    TRAIT_JINXED,
    TRAIT_GOOD_NATURED,
    TRAIT_CHEM_RELIANT,
    TRAIT_CHEM_RESISTANT,
    TRAIT_SEX_APPEAL,
    TRAIT_SKILLED,
    TRAIT_GIFTED,
    TRAIT_COUNT,
} Trait;

} // namespace fallout

#endif /* TRAIT_DEFS */
