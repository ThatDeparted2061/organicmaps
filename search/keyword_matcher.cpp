#include "keyword_matcher.hpp"

#include "../indexer/search_delimiters.hpp"
#include "../indexer/search_string_utils.hpp"

#include "../base/stl_add.hpp"

#include "../std/algorithm.hpp"

namespace search
{

KeywordMatcher::KeywordMatcher()
{
  Clear();
}

void KeywordMatcher::Clear()
{
  m_keywords = NULL;
  m_keywordsCount = 0;
  m_prefix = NULL;
}

void KeywordMatcher::SetKeywords(StringT const * keywords, size_t count, StringT const * prefix)
{
  m_keywords = keywords;
  m_keywordsCount = min(static_cast<size_t>(MAX_TOKENS), count);

  m_prefix = prefix;
  if (m_prefix && m_prefix->empty())
    m_prefix = NULL;
}

KeywordMatcher::ScoreT KeywordMatcher::Score(string const & name) const
{
  return Score(NormalizeAndSimplifyString(name));
}

KeywordMatcher::ScoreT KeywordMatcher::Score(StringT const & name) const
{
  buffer_vector<StringT, MAX_TOKENS> tokens;
  SplitUniString(name, MakeBackInsertFunctor(tokens), Delimiters());

  // Some names can have too many tokens. Trim them.
  return Score(tokens.data(), min(size_t(MAX_TOKENS), tokens.size()));
}

KeywordMatcher::ScoreT KeywordMatcher::Score(StringT const * tokens, size_t count) const
{
  vector<bool> isQueryTokenMatched(m_keywordsCount);
  vector<bool> isNameTokenMatched(count);
  uint32_t numQueryTokensMatched = 0;
  uint32_t sumTokenMatchDistance = 0;
  uint32_t prevTokenMatchDistance = 0;
  bool bPrefixMatched = true;

  for (int i = 0; i < m_keywordsCount; ++i)
    for (int j = 0; j < count && !isQueryTokenMatched[i]; ++j)
      if (!isNameTokenMatched[j] && m_keywords[i] == tokens[j])
      {
        isQueryTokenMatched[i] = isNameTokenMatched[j] = true;
        uint32_t const tokenMatchDistance = i - j;
        sumTokenMatchDistance += abs(tokenMatchDistance - prevTokenMatchDistance);
        prevTokenMatchDistance = tokenMatchDistance;
      }

  if (m_prefix)
  {
    bPrefixMatched = false;
    for (int j = 0; j < count && !bPrefixMatched; ++j)
      if (!isNameTokenMatched[j] &&
          StartsWith(tokens[j].begin(), tokens[j].end(), m_prefix->begin(), m_prefix->end()))
      {
        isNameTokenMatched[j] = bPrefixMatched = true;
        uint32_t const tokenMatchDistance = int(m_keywordsCount) - j;
        sumTokenMatchDistance += abs(tokenMatchDistance - prevTokenMatchDistance);
      }
  }

  for (size_t i = 0; i < isQueryTokenMatched.size(); ++i)
    if (isQueryTokenMatched[i])
      ++numQueryTokensMatched;

  ScoreT score = ScoreT();
  score.m_bFullQueryMatched = bPrefixMatched && (numQueryTokensMatched == isQueryTokenMatched.size());
  score.m_bPrefixMatched = bPrefixMatched;
  score.m_numQueryTokensAndPrefixMatched = numQueryTokensMatched + (bPrefixMatched ? 1 : 0);
  score.m_nameTokensMatched = 0xFFFFFFFF;
  for (uint32_t i = 0; i < min(size_t(32), isNameTokenMatched.size()); ++i)
    if (!isNameTokenMatched[i])
      score.m_nameTokensMatched &= ~(1 << (31 - i));
  score.m_sumTokenMatchDistance = sumTokenMatchDistance;

  return score;
}

KeywordMatcher::ScoreT::ScoreT()
  : m_sumTokenMatchDistance(0), m_nameTokensMatched(0), m_numQueryTokensAndPrefixMatched(0),
    m_bFullQueryMatched(false), m_bPrefixMatched(false)
{
}

bool KeywordMatcher::ScoreT::operator < (KeywordMatcher::ScoreT const & s) const
{
  if (m_bFullQueryMatched != s.m_bFullQueryMatched)
    return m_bFullQueryMatched < s.m_bFullQueryMatched;
  if (m_numQueryTokensAndPrefixMatched != s.m_numQueryTokensAndPrefixMatched)
    return m_numQueryTokensAndPrefixMatched < s.m_numQueryTokensAndPrefixMatched;
  if (m_bPrefixMatched != s.m_bPrefixMatched)
    return m_bPrefixMatched < s.m_bPrefixMatched;
  if (m_nameTokensMatched != s.m_nameTokensMatched)
    return m_nameTokensMatched < s.m_nameTokensMatched;
  if (m_sumTokenMatchDistance != s.m_sumTokenMatchDistance)
    return m_sumTokenMatchDistance > s.m_sumTokenMatchDistance;
  return false;
}

string DebugPrint(KeywordMatcher::ScoreT const & score)
{
  ostringstream out;
  out << "KeywordMatcher::ScoreT(";
  out << "FQM=" << score.m_bFullQueryMatched;
  out << ",nQTM=" << static_cast<int>(score.m_numQueryTokensAndPrefixMatched);
  out << ",PM=" << score.m_bPrefixMatched;
  out << ",NTM=";
  for (int i = 31; i >= 0; --i) out << ((score.m_nameTokensMatched >> i) & 1);
  out << ",STMD=" << score.m_sumTokenMatchDistance;
  out << ")";
  return out.str();
}

}  // namespace search
