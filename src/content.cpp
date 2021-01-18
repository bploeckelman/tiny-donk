#include "content.h"
#include "game.h"

using namespace Blah;
using namespace Zen;

namespace {
    struct RoomInfo {
        Image image;
        Point cell;
    };

    struct SpriteInfo {
        String name;
        Aseprite aseprite;
        uint64_t pack_index;
    };

    FilePath root;
    Vector<Sprite> sprites;
    Vector<Tileset> tilesets;
    Vector<Subtexture> subtextures;
    Vector<RoomInfo> rooms;
    TextureRef sprite_atlas;
}

SpriteFont Content::font;

FilePath Content::path() {
    if (root.length() <= 0) {
        FilePath up = "";
        do {
            root = Path::normalize(FilePath::fmt("%s/%scontent/", App::path(), up.cstr()));
            up.append("../");
        } while (!Directory::exists(root) && up.length() < 30);

        if (!Directory::exists(root)) {
            BLAH_ERROR("Unable to find content directory!");
        }

        Log::print("Content path: %s", root.cstr());
    }

    return root;
}

void Content::load() {
    Packer packer;
    packer.padding = 0;

    // load the main font
    font = SpriteFont(path() + "fonts/dogica.ttf", 8, SpriteFont::ASCII);
    font.line_gap = 4;

    // load sprites
    Vector<SpriteInfo> sprite_info;
    uint64_t pack_index = 0;
    {
        // get all the sprites
        FilePath sprite_path = path() + "sprites/";
        for (auto& it : Directory::enumerate(sprite_path, true)) {
            if (!it.ends_with(".ase")) continue;

            SpriteInfo* info = sprite_info.expand();
            info->name = String(it.cstr() + sprite_path.length(), it.end() - 4);
            info->aseprite = Aseprite(it.cstr());
            info->pack_index = pack_index;

            // add to the atlas
            for (auto& frame : info->aseprite.frames) {
                packer.add(pack_index, frame.image);
                pack_index++;
            }
        }
    }

    // load tilesets
    Vector<SpriteInfo> tileset_info;
    {
        // get all the tilesets
        FilePath tileset_path = path() + "tilesets/";
        for (auto& it : Directory::enumerate(tileset_path, true)) {
            if (!it.ends_with(".ase")) continue;

            SpriteInfo* info = tileset_info.expand();
            info->name = String(it.cstr() + tileset_path.length(), it.end() - 4);
            info->aseprite = Aseprite(it.cstr());
            info->pack_index = pack_index;

            auto& frame = info->aseprite.frames[0];
            auto columns = frame.image.width / Game::tile_width;
            auto rows = frame.image.height / Game::tile_height;

            // split into grid and add subimages to atlas
            for (int x = 0; x < columns; x++) {
                for (int y = 0; y < rows; y++) {
                    auto subrect = RectI(x * Game::tile_width, y * Game::tile_height, Game::tile_width, Game::tile_height);
                    auto subimage = frame.image.get_sub_image(subrect);
                    packer.add(pack_index, subimage);
                    pack_index++;
                }
            }
        }
    }

    // build the atlas
    {
        packer.pack();
        sprite_atlas = Texture::create(packer.pages[0]);

        subtextures.expand(packer.entries.size());
        for (auto& entry : packer.entries) {
            subtextures[entry.id] = Subtexture(sprite_atlas, entry.packed, entry.frame);
        }
    }

    // add sprites
    for (auto& info : sprite_info) {
        Sprite* sprite = sprites.expand();
        sprite->name = info.name;
        sprite->origin = Vec2::zero;

        if (info.aseprite.slices.size() > 0 && info.aseprite.slices[0].has_pivot) {
            sprite->origin = Vec2(
                    info.aseprite.slices[0].pivot.x,
                    info.aseprite.slices[0].pivot.y);
        }

        for (auto& tag : info.aseprite.tags) {
            Sprite::Animation* anim = sprite->animations.expand();
            anim->name = tag.name;

            for (int i = tag.from; i <= tag.to; i++) {
                Sprite::Frame* frame = anim->frames.expand();
                frame->duration = info.aseprite.frames[i].duration / 1000.0f; // ms -> sec
                frame->image = subtextures[info.pack_index + i];
            }
        }
    }

    // add tilesets
    for (auto& info : tileset_info) {
        auto& frame = info.aseprite.frames[0];

        Tileset* tileset = tilesets.expand();
        tileset->name = info.name;
        tileset->columns = frame.image.width / Game::tile_width;
        tileset->rows = frame.image.height / Game::tile_height;

        // split into grid and add subimages to atlas
        for (int x = 0, i = info.pack_index; x < tileset->columns; x++) {
            for (int y = 0; y < tileset->rows; y++) {
                tileset->tiles[x + y * tileset->columns] = subtextures[i];
                i++;
            }
        }
    }

    // load the rooms
    for (auto& it : Directory::enumerate(path() + "/map", false)) {
        if (!it.ends_with(".png")) continue;

        auto name = Path::get_file_name_no_ext(it);
        auto point = name.split('x');
        if (point.size() != 2) continue;

        RoomInfo info;
        info.cell.x = strtol(point[0].cstr(), nullptr, 10);
        info.cell.y = strtol(point[1].cstr(), nullptr, 10);
        info.image = Image(it);

        BLAH_ASSERT(info.image.width == Game::columns, "Room is incorrect height!");
        BLAH_ASSERT(info.image.height == Game::rows, "Room is incorrect height!");

        rooms.push_back(info);
    }
}

void Content::unload() {
    font.dispose();
}

TextureRef Content::atlas() {
    return sprite_atlas;
}

const Sprite* Content::find_sprite(const char *name) {
    for (auto& it : sprites) {
        if (it.name == name) {
            return &it;
        }
    }
    return nullptr;
}

const Tileset* Content::find_tileset(const char *name) {
    for (auto& it : tilesets) {
        if (it.name == name) {
            return &it;
        }
    }
    return nullptr;
}

const Image* Content::find_room(const Point &cell) {
    for (auto& it : rooms) {
        if (it.cell == cell) {
            return &it.image;
        }
    }
    return nullptr;
}
