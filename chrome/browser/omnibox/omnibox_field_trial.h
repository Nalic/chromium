// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_OMNIBOX_FIELD_TRIAL_H_
#define CHROME_BROWSER_OMNIBOX_OMNIBOX_FIELD_TRIAL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/common/autocomplete_match_type.h"

// This class manages the Omnibox field trials.
class OmniboxFieldTrial {
 public:
  // A mapping that contains multipliers indicating that matches of the
  // specified type should have their relevance score multiplied by the
  // given number.  Omitted types are assumed to have multipliers of 1.0.
  typedef std::map<AutocompleteMatchType::Type, float> DemotionMultipliers;

  // A set of types that should not be demoted when they are the top match.
  typedef std::set<AutocompleteMatchType::Type> UndemotableTopMatchTypes;

  // Creates the static field trial groups.
  // *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void ActivateStaticTrials();

  // Activates all dynamic field trials.  The main difference between
  // the autocomplete dynamic and static field trials is that the former
  // don't require any code changes on the Chrome side as they are controlled
  // on the server side.  Chrome binary simply propagates all necessary
  // information through the X-Client-Data header.
  // This method, unlike ActivateStaticTrials(), may be called multiple times.
  static void ActivateDynamicTrials();

  // Returns a bitmap containing AutocompleteProvider::Type values
  // that should be disabled in AutocompleteController.
  // This method simply goes over all autocomplete dynamic field trial groups
  // and looks for group names like "ProvidersDisabled_NNN" where NNN is
  // an integer corresponding to a bitmap mask.  All extracted bitmaps
  // are OR-ed together and returned as the final result.
  static int GetDisabledProviderTypes();

  // Returns whether the user is in any dynamic field trial where the
  // group has a the prefix |group_prefix|.
  static bool HasDynamicFieldTrialGroupPrefix(const char *group_prefix);

  // ---------------------------------------------------------
  // For the suggest field trial.

  // Populates |field_trial_hash| with hashes of the active suggest field trial
  // names, if any.
  static void GetActiveSuggestFieldTrialHashes(
      std::vector<uint32>* field_trial_hash);

  // ---------------------------------------------------------
  // For the HistoryURL provider disable culling redirects field trial.

  // Returns whether the user is in any group for this field trial.
  // (Should always be true unless initialization went wrong.)
  static bool InHUPCullRedirectsFieldTrial();

  // Returns whether we should disable culling of redirects in
  // HistoryURL provider.
  static bool InHUPCullRedirectsFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the HistoryURL provider disable creating a shorter match
  // field trial.

  // Returns whether the user is in any group for this field trial.
  // (Should always be true unless initialization went wrong.)
  static bool InHUPCreateShorterMatchFieldTrial();

  // Returns whether we should disable creating a shorter match in
  // HistoryURL provider.
  static bool InHUPCreateShorterMatchFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the AutocompleteController "stop timer" field trial.

  // Returns whether the user should get the experimental setup or the
  // default setup for this field trial.  The experiment group uses
  // a timer in AutocompleteController to tell the providers to stop
  // looking for matches after too much time has passed.  In other words,
  // it tries to tell the providers to stop updating the list of suggested
  // matches if updating the matches would probably be disruptive because
  // they're arriving so late.
  static bool InStopTimerFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the ZeroSuggestProvider field trial.

  // Returns whether the user is in any field trial where the
  // ZeroSuggestProvider should be used to get suggestions when the
  // user clicks on the omnibox but has not typed anything yet.
  static bool InZeroSuggestFieldTrial();

  // Returns whether the user is in a ZeroSuggest field trial, but should
  // show most visited URL instead.  This is used to compare metrics of
  // ZeroSuggest and most visited suggestions.
  static bool InZeroSuggestMostVisitedFieldTrial();

  // Returns whether the user is in a ZeroSuggest field trial and URL-based
  // suggestions can continue to appear after the user has started typing.
  static bool InZeroSuggestAfterTypingFieldTrial();

  // ---------------------------------------------------------
  // For the ShortcutsScoringMaxRelevance experiment that's part of the
  // bundled omnibox field trial.

  // If the user is in an experiment group that, given the provided
  // |current_page_classification| context, changes the maximum relevance
  // ShortcutsProvider::CalculateScore() is supposed to assign, extract
  // that maximum relevance score and put in in |max_relevance|.  Returns
  // true on a successful extraction.  CalculateScore()'s return value is
  // a product of this maximum relevance score and some attenuating factors
  // that are all between 0 and 1.  (Note that Shortcuts results may have
  // their scores reduced later if the assigned score is higher than allowed
  // for non-inlineable results.  Shortcuts results are not allowed to be
  // inlined.)
  static bool ShortcutsScoringMaxRelevance(
      AutocompleteInput::PageClassification current_page_classification,
      int* max_relevance);

  // ---------------------------------------------------------
  // For the SearchHistory experiment that's part of the bundled omnibox
  // field trial.

  // Returns true if the user is in the experiment group that, given the
  // provided |current_page_classification| context, scores search history
  // query suggestions less aggressively so that they don't inline.
  static bool SearchHistoryPreventInlining(
      AutocompleteInput::PageClassification current_page_classification);

  // Returns true if the user is in the experiment group that, given the
  // provided |current_page_classification| context, disables all query
  // suggestions from search history.
  static bool SearchHistoryDisable(
      AutocompleteInput::PageClassification current_page_classification);

  // ---------------------------------------------------------
  // For the DemoteByType experiment that's part of the bundled omnibox field
  // trial.

  // If the user is in an experiment group that, in the provided
  // |current_page_classification| context, demotes the relevance scores
  // of certain types of matches, populates the |demotions_by_type| map
  // appropriately.  Otherwise, clears |demotions_by_type|.
  static void GetDemotionsByType(
      AutocompleteInput::PageClassification current_page_classification,
      DemotionMultipliers* demotions_by_type);

  // Get the set of types that should not be demoted if they are the top
  // match.
  static UndemotableTopMatchTypes GetUndemotableTopTypes(
      AutocompleteInput::PageClassification current_page_classification);

  // ---------------------------------------------------------
  // For the ReorderForLegalDefaultMatch experiment that's part of the
  // bundled omnibox field trial.

  // Returns true if the omnibox will reorder matches, in the provided
  // |current_page_classification| context so that a match that's allowed to
  // be the default match will appear first.  This means AutocompleteProviders
  // can score matches however they desire without regard to making sure the
  // top match when all the matches from all providers are merged is a legal
  // default match.
  static bool ReorderForLegalDefaultMatch(
      AutocompleteInput::PageClassification current_page_classification);

  // ---------------------------------------------------------
  // For the HQPBookmarkValue experiment that's part of the
  // bundled omnibox field trial.

  // Returns the value an untyped visit to a bookmark should receive.
  // Compare this value with the default of 1 for non-bookmarked untyped
  // visits to pages and the default of 20 for typed visits.  Returns
  // 1 if the bookmark value experiment isn't active.
  static int HQPBookmarkValue();

  // ---------------------------------------------------------
  // For the HQPAllowMatchInTLD experiment that's part of the
  // bundled omnibox field trial.

  // Returns true if HQP should allow an input term to match in the
  // top level domain (e.g., .com) of a URL.  Returns false if the
  // allow match in TLD experiment isn't active.
  static bool HQPAllowMatchInTLDValue();

  // ---------------------------------------------------------
  // For the HQPAllowMatchInScheme experiment that's part of the
  // bundled omnibox field trial.

  // Returns true if HQP should allow an input term to match in the
  // scheme (e.g., http://) of a URL.  Returns false if the allow
  // match in scheme experiment isn't active.
  static bool HQPAllowMatchInSchemeValue();

  // ---------------------------------------------------------
  // Exposed publicly for the sake of unittests.
  static const char kBundledExperimentFieldTrialName[];
  // Rule names used by the bundled experiment.
  static const char kShortcutsScoringMaxRelevanceRule[];
  static const char kSearchHistoryRule[];
  static const char kDemoteByTypeRule[];
  static const char kUndemotableTopTypeRule[];
  static const char kReorderForLegalDefaultMatchRule[];
  static const char kHQPBookmarkValueRule[];
  static const char kHQPAllowMatchInTLDRule[];
  static const char kHQPAllowMatchInSchemeRule[];
  // Rule values.
  static const char kReorderForLegalDefaultMatchRuleEnabled[];

 private:
  friend class OmniboxFieldTrialTest;

  // The bundled omnibox experiment comes with a set of parameters
  // (key-value pairs).  Each key indicates a certain rule that applies in
  // a certain context.  The value indicates what the consequences of
  // applying the rule are.  For example, the value of a SearchHistory rule
  // in the context of a search results page might indicate that we should
  // prevent search history matches from inlining.
  //
  // This function returns the value associated with the |rule| that applies
  // in the current context (which currently consists of |page_classification|
  // and whether Instant Extended is enabled).  If no such rule exists in the
  // current context, fall back to the rule in various wildcard contexts and
  // return its value if found.  If the rule remains unfound in the global
  // context, returns the empty string.  For more details, including how we
  // prioritize different wildcard contexts, see the implementation.  How to
  // interpret the value is left to the caller; this is rule-dependent.
  static std::string GetValueForRuleInContext(
      const std::string& rule,
      AutocompleteInput::PageClassification page_classification);

  DISALLOW_IMPLICIT_CONSTRUCTORS(OmniboxFieldTrial);
};

#endif  // CHROME_BROWSER_OMNIBOX_OMNIBOX_FIELD_TRIAL_H_
