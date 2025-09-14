#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <gtkmm.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

class MusicPlayer : public Gtk::Window {
public:
    MusicPlayer()
    {
        set_default_size(560, 120);
        set_title("FMP Music Player");
        set_decorated(false);
        set_keep_above(true);
        set_resizable(false);

        overlay = Gtk::make_managed<Gtk::Overlay>();

        auto pixbuf = Gdk::Pixbuf::create_from_file("bg.png");
        background = Gtk::make_managed<Gtk::Image>();
        background->set(pixbuf->scale_simple(560, 120, Gdk::INTERP_BILINEAR));
        background->set_halign(Gtk::ALIGN_FILL);
        background->set_valign(Gtk::ALIGN_FILL);
        overlay->add(*background);

        main_box.set_orientation(Gtk::ORIENTATION_VERTICAL);
        main_box.set_spacing(8);
        main_box.set_halign(Gtk::ALIGN_CENTER);
        main_box.set_valign(Gtk::ALIGN_CENTER);

        label.set_text("Music player");
        main_box.pack_start(label, Gtk::PACK_SHRINK);

        box.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        box.set_spacing(8);
        box.set_halign(Gtk::ALIGN_CENTER);
        box.set_valign(Gtk::ALIGN_CENTER);

        btn_choose.set_label("Choose Folder");
        btn_choose.signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_choose_folder));
        box.pack_start(btn_choose, Gtk::PACK_SHRINK);

        btn_start.set_label("Start");
        btn_start.signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_start));
        box.pack_start(btn_start, Gtk::PACK_SHRINK);

        btn_pause.set_label("Pause");
        btn_pause.signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_pause));
        box.pack_start(btn_pause, Gtk::PACK_SHRINK);

        btn_next.set_label("Next");
        btn_next.signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_next));
        box.pack_start(btn_next, Gtk::PACK_SHRINK);

        btn_quit.set_label("Quit");
        btn_quit.signal_clicked().connect([](){ Gtk::Main::quit(); });
        box.pack_start(btn_quit, Gtk::PACK_SHRINK);

        volume_adjustment = Gtk::Adjustment::create(64.0, 0.0, 128.0, 1.0, 10.0, 0.0);
        volume_scale.set_adjustment(volume_adjustment);
        volume_scale.set_digits(0);
        volume_scale.set_value_pos(Gtk::PositionType::POS_TOP);
        volume_scale.set_size_request(120, -1);
        volume_scale.signal_value_changed().connect(sigc::mem_fun(*this, &MusicPlayer::on_volume_changed));
        box.pack_start(volume_scale, Gtk::PACK_SHRINK);

        main_box.pack_start(box, Gtk::PACK_SHRINK);

        overlay->add_overlay(main_box);
        add(*overlay);
        show_all_children();

        signal_button_press_event().connect(sigc::mem_fun(*this, &MusicPlayer::on_mouse_press), false);
        signal_motion_notify_event().connect(sigc::mem_fun(*this, &MusicPlayer::on_mouse_move), false);
        add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON1_MOTION_MASK);

        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        }

        int flags = MIX_INIT_MP3;
        if ((Mix_Init(flags) & flags) != flags) {
            std::cerr << "Mix_Init failed: " << Mix_GetError() << "\n";
        }

        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            std::cerr << "Mix_OpenAudio failed: " << Mix_GetError() << "\n";
        }

        current_volume = static_cast<int>(volume_adjustment->get_value());
        Mix_VolumeMusic(current_volume);

        Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &MusicPlayer::on_poll_music), 1);

        load_css("style.css");
    }

    ~MusicPlayer()
    {
        stop_music();
        Mix_CloseAudio();
        Mix_Quit();
        SDL_Quit();
    }

    void load_css(const std::string& css_file)
    {
        auto css_provider = Gtk::CssProvider::create();
        try {
            css_provider->load_from_path(css_file);
            auto screen = Gdk::Screen::get_default();
            Gtk::StyleContext::add_provider_for_screen(screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
        } catch (const Glib::FileError& e) {
            std::cerr << "CSS file error: " << e.what() << "\n";
        } catch (const Gtk::CssProviderError& e) {
            std::cerr << "CSS load error: " << e.what() << "\n";
        }
    }

    void set_background(const std::string& file)
    {
        try {
            auto pixbuf = Gdk::Pixbuf::create_from_file(file);
            background->set(pixbuf->scale_simple(480, 140, Gdk::INTERP_BILINEAR));
        } catch (const Glib::Error& e) {
            std::cerr << "Failed to load background: " << e.what() << "\n";
        }
    }

    void set_button_image(Gtk::Button& btn, const std::string& file)
    {
        try {
            auto pixbuf = Gdk::Pixbuf::create_from_file(file);
            auto img = Gtk::make_managed<Gtk::Image>(pixbuf->scale_simple(32, 32, Gdk::INTERP_BILINEAR));
            btn.set_image(*img);
            btn.set_always_show_image(true);
        } catch (const Glib::Error& e) {
            std::cerr << "Failed to load button image: " << e.what() << "\n";
        }
    }

private:
    Gtk::Overlay* overlay;
    Gtk::Box main_box;
    Gtk::Box box;
    Gtk::Label label;
    Gtk::Button btn_choose, btn_start, btn_pause, btn_next, btn_quit;
    Glib::RefPtr<Gtk::Adjustment> volume_adjustment;
    Gtk::Scale volume_scale;
    Gtk::Image* background;
    std::vector<std::string> playlist;
    size_t current_index = 0;
    Mix_Music* current_music = nullptr;
    bool paused = false;
    int current_volume = 96;

    int drag_offset_x = 0;
    int drag_offset_y = 0;

    bool on_mouse_press(GdkEventButton* event)
    {
        if (event->button == 1) {
            int win_x, win_y;
            get_position(win_x, win_y);
            drag_offset_x = event->x_root - win_x;
            drag_offset_y = event->y_root - win_y;
        }
        return true;
    }

    bool on_mouse_move(GdkEventMotion* event)
    {
        if (event->state & GDK_BUTTON1_MASK) {
            move(event->x_root - drag_offset_x, event->y_root - drag_offset_y);
        }
        return true;
    }

    void on_choose_folder()
    {
        Gtk::FileChooserDialog dialog(*this, "Choose Folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("_Select", Gtk::RESPONSE_OK);
        if (dialog.run() == Gtk::RESPONSE_OK) {
            load_playlist_from_folder(dialog.get_filename());
        }
    }

    void load_playlist_from_folder(const std::string& folder)
    {
        playlist.clear();
        current_index = 0;
        try {
            for (auto &p : fs::directory_iterator(folder)) {
                if (!p.is_regular_file()) continue;
                std::string ext = p.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mp3" || ext == ".ogg" || ext == ".wav") playlist.push_back(p.path().string());
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << "\n";
        }
        if (playlist.empty()) label.set_text("No audio files found");
        else label.set_text("Loaded " + std::to_string(playlist.size()) + " files");
    }

    void on_start()
    {
        if (playlist.empty()) { label.set_text("Playlist empty — choose folder"); return; }
        if (paused) { Mix_ResumeMusic(); paused = false; return; }
        play_current();
    }

    void on_pause()
    {
        if (Mix_PlayingMusic()) { Mix_PauseMusic(); paused = true; }
        else if (paused) { Mix_ResumeMusic(); paused = false; }
    }

    void on_next()
    {
        stop_music();
        next_index();
        play_current();
    }

    bool on_poll_music()
    {
        if (!playlist.empty() && !Mix_PlayingMusic() && !paused) {
            next_index();
            play_current();
        }
        return true;
    }

    void next_index()
    {
        if (playlist.empty()) return;
        current_index = (current_index + 1) % playlist.size();
    }

    void play_current()
    {
        stop_music();
        if (playlist.empty()) return;
        std::string path = playlist[current_index];
        current_music = Mix_LoadMUS(path.c_str());
        if (!current_music) { std::cerr << "Mix_LoadMUS failed for " << path << ": " << Mix_GetError() << "\n"; next_index(); return; }
        Mix_VolumeMusic(current_volume);
        if (Mix_PlayMusic(current_music, 1) < 0) { std::cerr << "Mix_PlayMusic failed: " << Mix_GetError() << "\n"; Mix_FreeMusic(current_music); current_music = nullptr; }
    }

    void stop_music()
    {
        if (current_music) { Mix_HaltMusic(); Mix_FreeMusic(current_music); current_music = nullptr; }
        paused = false;
    }

    void on_volume_changed()
    {
        current_volume = static_cast<int>(volume_adjustment->get_value());
        Mix_VolumeMusic(current_volume);
    }
};

int main(int argc, char **argv)
{
    auto app = Gtk::Application::create(argc, argv, "com.autoselff.fmp");
    MusicPlayer player;
    return app->run(player);
}
