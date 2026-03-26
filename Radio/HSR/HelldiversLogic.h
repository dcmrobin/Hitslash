#ifndef HELLDIVERS_LOGIC_H
#define HELLDIVERS_LOGIC_H

#include "DisplayLogic.h"
#include "HelperFunctions.h"

#define SEQ_LENGTH      5
#define MAX_OBJECTIVES  5
#define MAX_NEWS_ITEMS  3

extern String majorOrderTitle;
extern String majorOrderBrief;
extern String rewardText;

extern String objectiveText[MAX_OBJECTIVES];
extern int    objectiveProgress[MAX_OBJECTIVES];
extern int    objectiveTarget[MAX_OBJECTIVES];
extern int    objectiveCount;
extern int    contentHeight;

extern int           inputSequence[SEQ_LENGTH];
extern int           seqIndex;
extern unsigned long lastSeqPressTime;
extern const unsigned long seqTimeout;

extern int secretSequence[SEQ_LENGTH];

extern String hdTitle;
extern String hdBrief;
extern String hdTask;
extern int    hdProgress;
extern long   hdExpires;

extern String newsItems[MAX_NEWS_ITEMS];

extern unsigned long lastMajorOrderUpdate;
extern unsigned long lastNewsUpdate;
extern const unsigned long hdUpdateInterval;

extern unsigned long refreshPressStart;
extern unsigned long lastRefreshTrigger;
extern const unsigned long longPressTime;
extern const unsigned long refreshInterval;

extern int hdScrollOffset;

void fetchMajorOrder(bool force);
void fetchNews(bool force);
void registerSequence(int btn);
void handleHelldiversButtons();
void checkSecretSequence();
void parseMajorOrder(JsonDocument &doc);
void parseNews(JsonDocument &doc);

#endif // HELLDIVERS_LOGIC_H