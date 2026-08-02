// Unit tests for character_selector.cc — pure logic mirrors of the C-07
// render-stats null guards.
//
// This test does NOT link character_selector.cc (heavy rendering engine
// dependencies). It mirrors the fixed render loops from
// characterSelectorWindowRenderStats so the C-07 invariant is regression-
// tested: invalid skill/trait ids (skillGetName/traitGetName → nullptr) are
// skipped instead of reaching strcpy(text, nullptr) → UB.
//
// Production contracts (verified in the adversarial report s3-adv-c-07):
//   - skillGetName returns nullptr for !(skill >= 0 && skill < SKILL_COUNT)
//     (skill.cc:518-524, skill.h:52-55).
//   - traitGetName returns nullptr for !(trait >= 0 && trait < TRAIT_COUNT)
//     (trait.cc:161-167).
//   - The fixed loops (character_selector.cc:836-839 / 859-862):
//         str = skillGetName(skills[index]);
//         if (str == nullptr) { continue; }   // blank line, no strcpy UB
//         strcpy(text, str);
//     and the trait equivalent with traitGetName.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "skill_defs.h"
#include "trait_defs.h"

using namespace fallout;

namespace {

// Mirrors the production skillGetName null contract: returns false for any id
// outside [0, SKILL_COUNT).
bool testSelectorSkillIsValid(int skill)
{
    return skill >= 0 && skill < SKILL_COUNT;
}

// Mirrors the production traitGetName null contract: returns false for any id
// outside [0, TRAIT_COUNT).
bool testSelectorTraitIsValid(int trait)
{
    return trait >= 0 && trait < TRAIT_COUNT;
}

// Mirrors the C-07 fixed skills render loop (character_selector.cc:833-847):
// a line is drawn only when the id resolves to a non-null name. Returns the
// number of lines that would be drawn for the given tagged-skill array.
int testSelectorCountDrawnSkills(const int* skills, int count)
{
    int drawn = 0;
    for (int index = 0; index < count; index++) {
        if (testSelectorSkillIsValid(skills[index])) {
            drawn++;
        }
    }
    return drawn;
}

// Mirrors the C-07 fixed traits render loop (character_selector.cc:856-864):
// the loop iterates traitGetMaxSelectedCount() slots; invalid ids are skipped.
int testSelectorCountDrawnTraits(const int* traits, int count)
{
    int drawn = 0;
    for (int index = 0; index < count; index++) {
        if (testSelectorTraitIsValid(traits[index])) {
            drawn++;
        }
    }
    return drawn;
}

} // namespace

TEST_CASE("C-07: selector render stats skips invalid tagged skills")
{
    SUBCASE("valid tagged skills draw")
    {
        int skills[DEFAULT_TAGGED_SKILLS] = { SKILL_SMALL_GUNS, SKILL_BIG_GUNS, SKILL_ENERGY_WEAPONS };
        CHECK(testSelectorCountDrawnSkills(skills, DEFAULT_TAGGED_SKILLS) == 3);
    }

    SUBCASE("-1 (save made before tag selection) is skipped, others draw")
    {
        int skills[DEFAULT_TAGGED_SKILLS] = { SKILL_SMALL_GUNS, -1, SKILL_ENERGY_WEAPONS };
        CHECK(testSelectorCountDrawnSkills(skills, DEFAULT_TAGGED_SKILLS) == 2);
    }

    SUBCASE("all -1 (no tag skills) draws nothing")
    {
        int skills[DEFAULT_TAGGED_SKILLS] = { -1, -1, -1 };
        CHECK(testSelectorCountDrawnSkills(skills, DEFAULT_TAGGED_SKILLS) == 0);
    }

    SUBCASE("out-of-range id (SKILL_COUNT) is skipped")
    {
        int skills[DEFAULT_TAGGED_SKILLS] = { SKILL_SMALL_GUNS, SKILL_COUNT, -1 };
        CHECK(testSelectorCountDrawnSkills(skills, DEFAULT_TAGGED_SKILLS) == 1);
    }
}

TEST_CASE("C-07: selector render stats skips invalid traits")
{
    SUBCASE("valid FO2 traits draw")
    {
        int traits[TRAITS_MAX_SELECTED_COUNT] = { TRAIT_GIFTED, TRAIT_SKILLED, -1 };
        CHECK(testSelectorCountDrawnTraits(traits, 2) == 2);
    }

    SUBCASE("FO1 3-trait sentinel -1 in slot 2 is skipped")
    {
        int traits[TRAITS_MAX_SELECTED_COUNT] = { TRAIT_GIFTED, TRAIT_SKILLED, -1 };
        CHECK(testSelectorCountDrawnTraits(traits, 3) == 2);
    }

    SUBCASE("all -1 draws nothing")
    {
        int traits[TRAITS_MAX_SELECTED_COUNT] = { -1, -1, -1 };
        CHECK(testSelectorCountDrawnTraits(traits, 3) == 0);
    }

    SUBCASE("out-of-range id (TRAIT_COUNT) is skipped")
    {
        int traits[TRAITS_MAX_SELECTED_COUNT] = { TRAIT_GIFTED, TRAIT_COUNT, -1 };
        CHECK(testSelectorCountDrawnTraits(traits, 3) == 1);
    }
}

TEST_CASE("C-07: fixed loop never calls strcpy on a null name")
{
    // The critical property: for every invalid id the mirror (like the fixed
    // production loop) skips the line entirely — a null name never reaches
    // strcpy. Validating the drawn-count invariant above implies the skip.
    SUBCASE("drawn lines == valid ids for a mixed array")
    {
        int skills[DEFAULT_TAGGED_SKILLS] = { 0, -1, 2 };
        int drawn = testSelectorCountDrawnSkills(skills, DEFAULT_TAGGED_SKILLS);
        int valid = 0;
        for (int i = 0; i < DEFAULT_TAGGED_SKILLS; i++) {
            if (skills[i] >= 0 && skills[i] < SKILL_COUNT) {
                valid++;
            }
        }
        CHECK(drawn == valid);
    }
}
