#import "collection.h"

static bool apply_remote_state = FALSE;

extern void jiggyscore_setCollected(int levelid, int jiggy_id);
extern int jiggyscore_isCollected(int levelid, int jiggy_id);
extern void item_adjustByDiffWithoutHud(int item, int diff);
extern ActorMarker *func_8032B16C(enum jiggy_e jiggy_id);
extern void marker_despawn(ActorMarker *marker);
extern void chjiggy_hide(Actor *actor);
extern Actor *marker_getActor(ActorMarker *marker);

bool applying_remote_state()
{
    return apply_remote_state;
}

void collect_jiggy(int jiggy_enum_id, int collected_value)
{
    bool already_collected = jiggyscore_isCollected(jiggy_enum_id, collected_value);

    if (!already_collected)
    {
        ActorMarker *jiggy_marker = func_8032B16C(jiggy_enum_id);

        apply_remote_state = TRUE;
        jiggyscore_setCollected(jiggy_enum_id, collected_value);
        apply_remote_state = FALSE;

        item_adjustByDiffWithoutHud(ITEM_26_JIGGY_TOTAL, 1);

        if (jiggy_marker != NULL)
        {
            Actor *jiggy_actor = marker_getActor(jiggy_marker);
            if (jiggy_actor != NULL)
            {
                chjiggy_hide(jiggy_actor);
            }
        }
    }
}
