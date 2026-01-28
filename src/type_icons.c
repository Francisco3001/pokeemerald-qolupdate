#include "global.h"
#include "battle.h"
#include "decompress.h"
#include "graphics.h"
#include "pokedex.h"
#include "sprite.h"
#include "constants/pokemon.h"
#include "type_icons.h"
#include "config/battle.h"
#include "battle_anim.h"


// ============================================================================
// Assets: tenés que definirlos en src/graphics.c con INCBIN (ver abajo)
// ============================================================================
extern const u32 gBattleIcons_Gfx1[];
extern const u32 gBattleIcons_Gfx2[];
extern const u16 gBattleIcons_Pal1[];
extern const u16 gBattleIcons_Pal2[];
extern u8 gHealthboxSpriteIds[MAX_BATTLERS_COUNT];

static void GetTypeIconPos(u8 battler, s16 *x, s16 *y);
static bool8 sTypeIconsGfxLoaded;


// Tags propios (constantes, no macros de expansion)
#define TYPE_ICON_TAG_1  0x7A10
#define TYPE_ICON_TAG_2  0x7A11

#define SPRITE_NONE_ID   0xFF

// Posición (single). Ajustamos después si querés.
#define OPP_ICON_X  20
#define OPP_ICON_Y  26
#define ICON_Y_GAP  11

static u8 sTypeIconSpriteIds[MAX_BATTLERS_COUNT][2];

static const struct OamData sOamData_TypeIcon =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(8x16),
    .size = SPRITE_SIZE(8x16),
    .priority = 1,
};

static const struct SpritePalette sTypeIconPal1 =
{
    .data = gBattleIcons_Pal1,
    .tag = TYPE_ICON_TAG_1,
};

static const struct SpritePalette sTypeIconPal2 =
{
    .data = gBattleIcons_Pal2,
    .tag = TYPE_ICON_TAG_2,
};

static const struct CompressedSpriteSheet sSpriteSheet_TypeIcons1 =
{
    .data = gBattleIcons_Gfx1,
    .size = 64 * 10, // 640
    .tag = TYPE_ICON_TAG_1,
};

static const struct CompressedSpriteSheet sSpriteSheet_TypeIcons2 =
{
    .data = gBattleIcons_Gfx2,
    .size = 64 * 9, // 576
    .tag = TYPE_ICON_TAG_2,
};

static void SpriteCB_TypeIcon(struct Sprite *sprite)
{
    // Siempre visible, sin lógica extra.
}

static const struct SpriteTemplate sSpriteTemplate_TypeIcon1 =
{
    .tileTag = TYPE_ICON_TAG_1,
    .paletteTag = TYPE_ICON_TAG_1,
    .oam = &sOamData_TypeIcon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_TypeIcon,
};

static const struct SpriteTemplate sSpriteTemplate_TypeIcon2 =
{
    .tileTag = TYPE_ICON_TAG_2,
    .paletteTag = TYPE_ICON_TAG_2,
    .oam = &sOamData_TypeIcon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_TypeIcon,
};

static void EnsureTypeIconGfxLoaded(void)
{
    if (!sTypeIconsGfxLoaded)
    {
        LoadCompressedSpriteSheet(&sSpriteSheet_TypeIcons1);
        LoadSpritePalette(&sTypeIconPal1);
        LoadCompressedSpriteSheet(&sSpriteSheet_TypeIcons2);
        LoadSpritePalette(&sTypeIconPal2);
        sTypeIconsGfxLoaded = TRUE;
    }
}



static void DestroyBattlerTypeIcons(u8 battler)
{
    u8 i;
    for (i = 0; i < 2; i++)
    {
        u8 spriteId = sTypeIconSpriteIds[battler][i];
        if (spriteId != SPRITE_NONE_ID && spriteId < MAX_SPRITES)
        {
            if (gSprites[spriteId].inUse)
            {
                FreeSpriteOamMatrix(&gSprites[spriteId]);
                DestroySprite(&gSprites[spriteId]);
            }
        }
        sTypeIconSpriteIds[battler][i] = SPRITE_NONE_ID;
    }
}

static bool32 ShouldShowForSpecies(u16 species)
{
    return (B_SHOW_TYPES == SHOW_TYPES_ALWAYS);
}

// Sheet1: NORMAL..STEEL + MYSTERY
static bool32 IsTypeOnSheet1(u8 typeId)
{
    return (typeId <= TYPE_STEEL) || (typeId == TYPE_MYSTERY);
}

// Frame index dentro del sheet
static u8 GetFrameIndexForType(u8 typeId)
{
    if (typeId == TYPE_MYSTERY)
        return 9;

    if (IsTypeOnSheet1(typeId))
        return typeId;

    // Sheet2 empieza en TYPE_FIRE
    return (u8)(typeId - TYPE_FIRE);
}

static void SetTypeIconFrame(struct Sprite *sprite, u8 typeId)
{
    u8 frame = GetFrameIndexForType(typeId);
    u16 base = GetSpriteTileStartByTag(sprite->template->tileTag);
    sprite->oam.tileNum = base + frame * 2; // 2 tiles por frame (8x16)
}

static bool32 IsValidType(u8 type)
{
    // En Emerald vanilla: TYPE_NONE no existe, el rango es 0..TYPE_MYSTERY (normalmente 0..18)
    return (type <= TYPE_MYSTERY);
}

void LoadTypeIcons(u32 battler)
{
    struct Pokemon *mon;
    u16 species;
    u8 t1, t2;

    // Obtener el Pokémon real de ese battler
    if (GetBattlerSide(battler) == B_SIDE_PLAYER)
        mon = &gPlayerParty[gBattlerPartyIndexes[battler]];
    else
        mon = &gEnemyParty[gBattlerPartyIndexes[battler]];

    species = GetMonData(mon, MON_DATA_SPECIES);
    if (species == SPECIES_NONE)
        return;

    // Tipos desde SpeciesInfo (NO desde gBattleMons)
    t1 = gSpeciesInfo[species].types[0];
    t2 = gSpeciesInfo[species].types[1];

    LoadTypeIconsEx(battler, species, t1, t2);
}



static void GetTypeIconPos(u8 battler, s16 *x, s16 *y)
{
    u8 hbSpriteId;
    s16 hbX;
    s16 hbY;

    hbSpriteId = gHealthboxSpriteIds[battler];

    if (hbSpriteId == SPRITE_NONE_ID || hbSpriteId >= MAX_SPRITES || !gSprites[hbSpriteId].inUse)
    {
        *x = -32;
        *y = -32;
        return;
    }

    hbX = gSprites[hbSpriteId].x;
    hbY = gSprites[hbSpriteId].y;

    if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        // rival
        *x = hbX - 30;   // 4 px más a la izquierda
        *y = hbY - 8;   // 4 px más abajo
    }
    else
    {
        // jugador
        *x = hbX - 22;
        *y = hbY + 3;
    }
}

void LoadTypeIconsEx(u32 battler, u16 species, u8 t1, u8 t2)
{
    s16 x, y;

    if (species == SPECIES_NONE)
        return;

    if (!ShouldShowForSpecies(species))
        return;

    GetTypeIconPos(battler, &x, &y);
    if (x < 0 || y < 0)
        return;

    EnsureTypeIconGfxLoaded();
    DestroyBattlerTypeIcons(battler);

    // Icono 1
    {
        const struct SpriteTemplate *tpl;
        u8 spriteId;

        tpl = IsTypeOnSheet1(t1) ? &sSpriteTemplate_TypeIcon1 : &sSpriteTemplate_TypeIcon2;
        spriteId = CreateSpriteAtEnd(tpl, x, y, 0);

        if (spriteId != MAX_SPRITES)
        {
            SetTypeIconFrame(&gSprites[spriteId], t1);
            sTypeIconSpriteIds[battler][0] = spriteId;
        }
        else
        {
            sTypeIconSpriteIds[battler][0] = SPRITE_NONE_ID;
        }
    }

    // Icono 2
    if (t2 != t1)
    {
        const struct SpriteTemplate *tpl;
        u8 spriteId;

        tpl = IsTypeOnSheet1(t2) ? &sSpriteTemplate_TypeIcon1 : &sSpriteTemplate_TypeIcon2;
        spriteId = CreateSpriteAtEnd(tpl, x, y + ICON_Y_GAP, 0);

        if (spriteId != MAX_SPRITES)
        {
            SetTypeIconFrame(&gSprites[spriteId], t2);
            sTypeIconSpriteIds[battler][1] = spriteId;
        }
        else
        {
            sTypeIconSpriteIds[battler][1] = SPRITE_NONE_ID;
        }
    }
    else
    {
        sTypeIconSpriteIds[battler][1] = SPRITE_NONE_ID;
    }
}

void FreeTypeIconGfx(void)
{
    u8 b;

    for (b = 0; b < MAX_BATTLERS_COUNT; b++)
        DestroyBattlerTypeIcons(b);

    FreeSpriteTilesByTag(TYPE_ICON_TAG_1);
    FreeSpritePaletteByTag(TYPE_ICON_TAG_1);
    FreeSpriteTilesByTag(TYPE_ICON_TAG_2);
    FreeSpritePaletteByTag(TYPE_ICON_TAG_2);

    sTypeIconsGfxLoaded = FALSE;
}

void FreeTypeIconsGfx(void)
{
    FreeTypeIconGfx();
}

void ResetTypeIconsGfxState(void)
{
    sTypeIconsGfxLoaded = FALSE;
}

void DestroyAllTypeIcons(void)
{
    u8 b;
    for (b = 0; b < MAX_BATTLERS_COUNT; b++)
        DestroyBattlerTypeIcons(b);
}
void ResetTypeIconsState(void)
{
    memset(sTypeIconSpriteIds, SPRITE_NONE_ID, sizeof(sTypeIconSpriteIds));
}