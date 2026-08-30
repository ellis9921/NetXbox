#include "browser.h"
#include "image.h"
#include "translate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char* HOME_HTML =
    "<!DOCTYPE html>"
    "<html><head><title>NetXbox - Home</title></head>"
    "<body bgcolor=\"#1a1a2e\" style=\"font-family:sans-serif;color:#ffffff;\">"
    "<div style=\"text-align:center;padding:60px 20px;\">"
    "<h1 style=\"color:#e94560;font-size:48px;\">NetXbox Browser</h1>"
    "<p style=\"color:#808080;font-size:18px;\">Fast browsing for modded Xbox 360</p>"
    "<div style=\"margin:40px auto;max-width:600px;\">"
    "<form action=\"https://www.mojeek.com/search/\" method=\"GET\">"
    "<input type=\"text\" name=\"q\" placeholder=\"Search the web...\" "
    "style=\"width:80%;padding:12px;font-size:18px;border:2px solid #e94560;"
    "background:#16213e;color:#ffffff;border-radius:4px 0 0 4px;\">"
    "<input type=\"submit\" value=\"Search\" "
    "style=\"padding:12px 24px;font-size:18px;background:#e94560;color:#ffffff;"
    "border:2px solid #e94560;border-radius:0 4px 4px 0;cursor:pointer;\">"
    "</form>"
    "</div>"
    "<div style=\"margin-top:40px;\">"
    "<a href=\"https://www.mojeek.com/\" style=\"color:#e94560;margin:0 15px;font-size:16px;\">Mojeek</a>"
    "<a href=\"http://www.wikipedia.org\" style=\"color:#e94560;margin:0 15px;font-size:16px;\">Wikipedia</a>"
    "<a href=\"http://www.youtube.com\" style=\"color:#e94560;margin:0 15px;font-size:16px;\">YouTube</a>"
    "<a href=\"http://www.github.com\" style=\"color:#e94560;margin:0 15px;font-size:16px;\">GitHub</a>"
    "</div>"
    "</div></body></html>";

static const char* DEFAULT_FILTER_LIST =
    "! NetXbox default ad block list\n"
    "||doubleclick.net^\n"
    "||googlesyndication.com^\n"
    "||googleadservices.com^\n"
    "||googleadapis.com^\n"
    "||googletagmanager.com^\n"
    "||googlesyndics.com^\n"
    "||adnxs.com^\n"
    "||adsrvr.org^\n"
    "||demdex.net^\n"
    "||amazon-adsystem.com^\n"
    "||facebook.net/tr^\n"
    "||facebook.com/tr^\n"
    "||analytics.twitter.com^\n"
    "||ads.linkedin.com^\n"
    "||outbrain.com^\n"
    "||taboola.com^\n"
    "||criteo.com^\n"
    "||criteo.net^\n"
    "||pubmatic.com^\n"
    "||rubiconproject.com^\n"
    "||openx.net^\n"
    "||casalemedia.com^\n"
    "||indexexchange.com^\n"
    "||moatads.com^\n"
    "||scorecardresearch.com^\n"
    "||quantserve.com^\n"
    "||bluekai.com^\n"
    "||exelator.com^\n"
    "||krxd.net^\n"
    "||bounceexchange.com^\n"
    "||chartbeat.com^\n"
    "||optimizely.com^\n"
    "||segment.io^\n"
    "||branch.io^\n"
    "||hotjar.com^\n"
    "||mouseflow.com^\n"
    "||fullstory.com^\n"
    "||mixpanel.com^\n"
    "||amplitude.com^\n"
    "||sentry.io^\n"
    "||newrelic.com^\n"
    "||doubleclick.net^\n"
    "||ad.doubleclick.net^\n"
    "||pagead2.googlesyndication.com^\n"
    "||tpc.googlesyndication.com^\n"
    "||static.criteo.net^\n"
    "||assets.sc-static.net^\n"
    "||sb.scorecardresearch.com^\n"
    "||bat.bing.com^\n"
    "||snap.licdn.com^\n"
    "||pixel.facebook.com^\n"
    "||insurads.com^\n"
    "||narrativ.com^\n"
    "||medyanetads.com^\n"
    "||ad.thinkhub.jp^\n"
    "||adinall.com^\n"
    "||ad-mox.com^\n"
    "||ad-specs.guityo.jp^\n"
    "||ad_stir.com^\n"
    "||adap.tv^$third-party\n"
    "||advertising.com^$third-party\n"
    "||bidswitch.net^$third-party\n"
    "||blueconic.net^$third-party\n"
    "||borader.pluginare.com^$third-party\n"
    "||brealtime.com^$third-party\n"
    "||chorusstack.com^$third-party\n"
    "||connatix.com^$third-party\n"
    "||convertro.com^$third-party\n"
    "||crosspixel.net^$third-party\n"
    "||crwdcntrl.net^$third-party\n"
    "||cvx.io^$third-party\n"
    "||dataxu.com^$third-party\n"
    "||districtm.ca^$third-party\n"
    "||districtm.io^$third-party\n"
    "||dotomi.com^$third-party\n"
    "||doubleverify.com^$third-party\n"
    "||everesttech.net^$third-party\n"
    "||eyereturn.com^$third-party\n"
    "||eyeota.net^$third-party\n"
    "||firebaselogging-pa.googleapis.com^\n"
    "||flashtalking.com^$third-party\n"
    "||gemius.pl^$third-party\n"
    "||globalwebindex.net^$third-party\n"
    "||go-1bos2track.com^$third-party\n"
    "||goemsono.com^$third-party\n"
    "||goviral-media.com^$third-party\n"
    "||hariken.co^$third-party\n"
    "||hubapi.com^$third-party\n"
    "||imrworldwide.com^$third-party\n"
    "||indieclick.com^$third-party\n"
    "||insurads.com^$third-party\n"
    "||intentmedia.net^$third-party\n"
    "||juicyads.com^$third-party\n"
    "||koldetchafe.com^$third-party\n"
    "||krxd.net^$third-party\n"
    "||lijit.com^$third-party\n"
    "||liveintent.com^$third-party\n"
    "||m2db.com^$third-party\n"
    "||mathtag.com^$third-party\n"
    "||mdotm.com^$third-party\n"
    "||media.net^$third-party\n"
    "||media6degrees.com^$third-party\n"
    "||megapush.com^$third-party\n"
    "||mixpo.com^$third-party\n"
    "||narrativ.com^$third-party\n"
    "||nativo.com^$third-party\n"
    "||netaffiliation.com^$third-party\n"
    "||netmng.com^$third-party\n"
    "||neustat.biz^$third-party\n"
    "||nlkais.com^$third-party\n"
    "||nocturno.net^$third-party\n"
    "||noptel.com^$third-party\n"
    "||oggifinagle.com^$third-party\n"
    "||onaudience.com^$third-party\n"
    "||onechina.cn^$third-party\n"
    "||onlinedirect.com^$third-party\n"
    "||optkit.com^$third-party\n"
    "||optmnstr.com^$third-party\n"
    "||optnmstr.com^$third-party\n"
    "||pardot.com^$third-party\n"
    "||perfectaudience.com^$third-party\n"
    "||pippio.com^$third-party\n"
    "||plladblock.com^$third-party\n"
    "||popads.net^$third-party\n"
    "||popcash.net^$third-party\n"
    "||propellerads.com^$third-party\n"
    "||pubmatic.com^$third-party\n"
    "||pulse360.com^$third-party\n"
    "||pushame.com^$third-party\n"
    "||pushwoosh.com^$third-party\n"
    "||quantserve.com^$third-party\n"
    "||rdsgg.com^$third-party\n"
    "||realytics.net^$third-party\n"
    "||reproio.com^$third-party\n"
    "||revjet.com^$third-party\n"
    "||rtbhouse.com^$third-party\n"
    "||sail-horizon.com^$third-party\n"
    "||sc-static.net^$third-party\n"
    "||scootstreet.com^$third-party\n"
    "||serving-sys.com^$third-party\n"
    "||sharethrough.com^$third-party\n"
    "||simplicitasoft.com^$third-party\n"
    "||skimresources.com^$third-party\n"
    "||smartadserver.com^$third-party\n"
    "||smartstream.tv^$third-party\n"
    "||sociomantic.com^$third-party\n"
    "||sonobi.com^$third-party\n"
    "||spotxchange.com^$third-party\n"
    "||stickyadstv.com^$third-party\n"
    "||supersonicads.com^$third-party\n"
    "||survata.com^$third-party\n"
    "||taboola.com^$third-party\n"
    "||tapad.com^$third-party\n"
    "||teads.tv^$third-party\n"
    "||tidaltv.com^$third-party\n"
    "||trackinglibrary.com^$third-party\n"
    "||trafficjunky.com^$third-party\n"
    "||tribalfusion.com^$third-party\n"
    "||trkn.us^$third-party\n"
    "||turn.com^$third-party\n"
    "||ubembed.com^$third-party\n"
    "||undertone.com^$third-party\n"
    "||vidible.tv^$third-party\n"
    "||viewablemedia.net^$third-party\n"
    "||visiblemeasures.com^$third-party\n"
    "||voluum.com^$third-party\n"
    "||vtracking.cn^$third-party\n"
    "||web.stattrack.com^$third-party\n"
    "||webpower.eu^$third-party\n"
    "||wweprint.com^$third-party\n"
    "||yieldmo.com^$third-party\n"
    "||zeotap.com^$third-party\n"
    "||zqtk.net^$third-party\n"
    "||2leep.com^$third-party\n"
    "||33across.com^$third-party\n"
    "||adcolony.com^$third-party\n"
    "||adform.com^$third-party\n"
    "||adroll.com^$third-party\n"
    "||adsymptotic.com^$third-party\n"
    "||afftrack.com^$third-party\n"
    "||agkn.com^$third-party\n"
    "||airpush.com^$third-party\n"
    "||alkutalkarabia.com^$third-party\n"
    "||adıklama.com^$third-party\n"
    "||appier.net^$third-party\n"
    "||assoc-amazon.com^$third-party\n"
    "||b2c.com^$third-party\n"
    "||bannerflow.com^$third-party\n"
    "||bbysteam.com^$third-party\n"
    "||beopinion.com^$third-party\n"
    "||brealtime.com^$third-party\n"
    "||c1exchange.com^$third-party\n"
    "||cogocast.com^$third-party\n"
    "||converto.com^$third-party\n"
    "||crwdcntrl.net^$third-party\n"
    "||datx.co^$third-party\n"
    "||demandz.com^$third-party\n"
    "||digidip.net^$third-party\n"
    "||doubleclick.net^$third-party\n"
    "||dvtelectronics.com^$third-party\n"
    "||edgeads.net^$third-party\n"
    "||emxdgt.com^$third-party\n"
    "||epeeks.com^$third-party\n"
    "||eurostyle-express.com^$third-party\n"
    "||exoclick.com^$third-party\n"
    "||ezoic.net^$third-party\n"
    "||faping.com^$third-party\n"
    "||foxnetworks.com^$third-party\n"
    "||freewheel.com^$third-party\n"
    "||freewheel.tv^$third-party\n"
    "||fwmrm.net^$third-party\n"
    "||gumgum.com^$third-party\n"
    "||hsk.io^$third-party\n"
    "||humorwav.com^$third-party\n"
    "||idsintel.com^$third-party\n"
    "||impact-ad.jp^$third-party\n"
    "||imrworldwide.com^$third-party\n"
    "||intentmedia.net^$third-party\n"
    "||intermarkets.net^$third-party\n"
    "||interwnd.com^$third-party\n"
    "||juicyads.com^$third-party\n"
    "||komoona.com^$third-party\n"
    "||lastampaadulti.it^$third-party\n"
    "||lead可视.com^$third-party\n"
    "||lijit.com^$third-party\n"
    "||m9313.com^$third-party\n"
    "||mookie1.com^$third-party\n"
    "||nativeserv.com^$third-party\n"
    "||nectarineads.com^$third-party\n"
    "||netmng.com^$third-party\n"
    "||nompulse.com^$third-party\n"
    "||nxtck.com^$third-party\n"
    "||onespot.com^$third-party\n"
    "||ooochurch.com^$third-party\n"
    "||optimatic.com^$third-party\n"
    "||otm-r.com^$third-party\n"
    "||plista.com^$third-party\n"
    "||prebidjsx.com^$third-party\n"
    "||prexbyte.com^$third-party\n"
    "||pubmatic.com^$third-party\n"
    "||pushlinck.com^$third-party\n"
    "||q1connect.com^$third-party\n"
    "||quantserve.com^$third-party\n"
    "||radiumone.com^$third-party\n"
    "||rhetashopping.com^$third-party\n"
    "||roimedia.com^$third-party\n"
    "||rtbidme.com^$third-party\n"
    "||salesmartly.com^$third-party\n"
    "||sape.ru^$third-party\n"
    "||sascdn.com^$third-party\n"
    "||saymedia.com^$third-party\n"
    "||scarabresearch.com^$third-party\n"
    "||schonalternativ.com^$third-party\n"
    "||sectary.net^$third-party\n"
    "||servebom.com^$third-party\n"
    "||servecontent.com^$third-party\n"
    "||serving-sys.com^$third-party\n"
    "||sexinyourcity.com^$third-party\n"
    "||sharethis.com^$third-party\n"
    "||simply.com^$third-party\n"
    "||sitescout.com^$third-party\n"
    "||smartadserver.com^$third-party\n"
    "||smartclip.net^$third-party\n"
    "||smitions.com^$third-party\n"
    "||sparkstudios.com^$third-party\n"
    "||sportsmole.co.uk^$third-party\n"
    "||strossle.com^$third-party\n"
    "||styria-digital.com^$third-party\n"
    "||survata.com^$third-party\n"
    "||switchadhub.com^$third-party\n"
    "||sxo.pt^$third-party\n"
    "||targetix.net^$third-party\n"
    "||tbcdn.net^$third-party\n"
    "||teads.tv^$third-party\n"
    "||thenl.com^$third-party\n"
    "||theshippingbox.com^$third-party\n"
    "||tillvannas.se^$third-party\n"
    "||trafficjunky.net^$third-party\n"
    "||tribalfusion.com^$third-party\n"
    "||turn.com^$third-party\n"
    "||vcmedia.com^$third-party\n"
    "||vergic.com^$third-party\n"
    "||videoplaza.com^$third-party\n"
    "||videoplaza.tv^$third-party\n"
    "||viewbix.com^$third-party\n"
    "||volusion.com^$third-party\n"
    "||wapclick.com^$third-party\n"
    "||webtradehub.com^$third-party\n"
    "||widdit.com^$third-party\n"
    "||wips.com^$third-party\n"
    "||wishabi.com^$third-party\n"
    "||wonderpl.com^$third-party\n"
    "||xlivrdr.com^$third-party\n"
    "||xrea.com^$third-party\n"
    "||yieldmo.com^$third-party\n"
    "||yume.com^$third-party\n"
    "||zedo.com^$third-party\n"
    "||zergnet.com^$third-party\n"
    "||zryydi.com^$third-party\n";

void browser_init(BrowserState* state) {
    memset(state, 0, sizeof(BrowserState));
    state->tabs = (BrowserTab*)calloc(BROWSER_MAX_TABS, sizeof(BrowserTab));
    state->tab_count = 0;
    state->active_tab = 0;
    state->next_tab_id = 1;
    state->status_text = string_create("Ready");
    state->search_engine_url = string_create("https://www.mojeek.com/search?q=");
    state->show_bookmarks_bar = true;
    state->viewport_width = 1280;
    state->viewport_height = 720;
    state->ui_bar_height = 70;

    adblock_init(&state->adblock);
    adblock_load_from_string(&state->adblock, DEFAULT_FILTER_LIST);

    browser_create_tab(state, BROWSER_HOME_URL);
}

void browser_shutdown(BrowserState* state) {
    if (!state) return;
    for (int i = 0; i < state->tab_count; i++) {
        BrowserTab* tab = &state->tabs[i];
        string_free(&tab->url);
        string_free(&tab->title);
        string_free(&tab->search_query);
        if (tab->document) html_document_free(tab->document);
        render_layout_free(&tab->layout);
        if (tab->http_client) http_client_destroy(tab->http_client);
        if (tab->image_pixels) free(tab->image_pixels);
    }
    if (state->tabs) free(state->tabs);
    string_free(&state->status_text);
    string_free(&state->search_engine_url);
    for (int i = 0; i < state->bookmark_count; i++) {
        string_free(&state->bookmarks[i].name);
        string_free(&state->bookmarks[i].url);
    }
    for (int i = 0; i < state->history_count; i++) {
        string_free(&state->history[i].url);
        string_free(&state->history[i].title);
    }
    memset(state, 0, sizeof(BrowserState));
}

BrowserTab* browser_get_active_tab(BrowserState* state) {
    if (state->tab_count == 0 || state->active_tab >= state->tab_count)
        return NULL;
    return &state->tabs[state->active_tab];
}

BrowserTab* browser_create_tab(BrowserState* state, const char* url) {
    if (state->tab_count >= BROWSER_MAX_TABS) return NULL;

    BrowserTab* tab = &state->tabs[state->tab_count];
    memset(tab, 0, sizeof(BrowserTab));
    tab->id = state->next_tab_id++;
    tab->url = string_create("");
    tab->title = string_create("New Tab");
    tab->search_query = string_create("");
    tab->state = TAB_STATE_BLANK;
    tab->is_home_page = false;
    state->tab_count++;
    state->active_tab = state->tab_count - 1;

    if (url) browser_navigate(tab, url);
    return tab;
}

void browser_close_tab(BrowserState* state, int tab_index) {
    if (tab_index < 0 || tab_index >= state->tab_count) return;

    BrowserTab* tab = &state->tabs[tab_index];
    string_free(&tab->url);
    string_free(&tab->title);
    string_free(&tab->search_query);
    if (tab->document) html_document_free(tab->document);
    render_layout_free(&tab->layout);
    if (tab->http_client) http_client_destroy(tab->http_client);

    if (tab_index < state->tab_count - 1) {
        memmove(&state->tabs[tab_index], &state->tabs[tab_index + 1],
                (state->tab_count - tab_index - 1) * sizeof(BrowserTab));
    }
    state->tab_count--;

    if (state->tab_count == 0) {
    browser_create_tab(state, BROWSER_HOME_URL);
    } else if (state->active_tab >= state->tab_count) {
        state->active_tab = state->tab_count - 1;
    }
}

void browser_switch_tab(BrowserState* state, int tab_index) {
    if (tab_index >= 0 && tab_index < state->tab_count) {
        state->active_tab = tab_index;
    }
}

const char* browser_resolve_url(BrowserState* state, const char* input) {
    static char resolved[2048];

    if (!input || strlen(input) == 0) {
        return BROWSER_HOME_URL;
    }

    if (strncmp(input, "about:", 6) == 0) {
        return input;
    }

    if (strncmp(input, "http://", 7) == 0 || strncmp(input, "https://", 8) == 0) {
        strncpy(resolved, input, sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = '\0';
        return resolved;
    }

    if (strstr(input, ".") && !strstr(input, " ")) {
        _snprintf(resolved, sizeof(resolved), "http://%s", input);
        return resolved;
    }

    _snprintf(resolved, sizeof(resolved), "%s%s",
              state->search_engine_url.data, input);
    return resolved;
}

static void browser_load_home_page(BrowserTab* tab) {
    if (tab->document) html_document_free(tab->document);
    render_layout_free(&tab->layout);

    tab->document = html_parse(HOME_HTML, (int)strlen(HOME_HTML));
    tab->is_home_page = true;

    if (tab->title.data) string_free(&tab->title);
    tab->title = string_create("NetXbox - Home");
    tab->state = TAB_STATE_LOADED;
}

void browser_navigate(BrowserTab* tab, const char* url) {
    if (!tab || !url) return;

    if (tab->http_client) {
        http_client_destroy(tab->http_client);
        tab->http_client = NULL;
    }
    if (tab->document) {
        html_document_free(tab->document);
        tab->document = NULL;
    }
    render_layout_free(&tab->layout);
    if (tab->image_pixels) { free(tab->image_pixels); tab->image_pixels = NULL; }
    tab->is_home_page = false;
    tab->scroll_x = 0;
    tab->scroll_y = 0;

    string_free(&tab->url);
    tab->url = string_create(url);

    if (strcmp(url, BROWSER_HOME_URL) == 0) {
        browser_load_home_page(tab);
        return;
    }

    tab->http_client = http_client_create();
    if (http_client_request(tab->http_client, url, HTTP_METHOD_GET)) {
        tab->state = TAB_STATE_LOADING;
    } else {
        tab->state = TAB_STATE_ERROR;
        string_free(&tab->title);
        tab->title = string_create("Failed to connect");
    }
}

void browser_go_back(BrowserState* state) {
    if (state->history_pos > 0) {
        state->history_pos--;
        BrowserTab* tab = browser_get_active_tab(state);
        if (tab) {
            browser_navigate(tab, state->history[state->history_pos].url.data);
        }
    }
}

void browser_go_forward(BrowserState* state) {
    if (state->history_pos < state->history_count - 1) {
        state->history_pos++;
        BrowserTab* tab = browser_get_active_tab(state);
        if (tab) {
            browser_navigate(tab, state->history[state->history_pos].url.data);
        }
    }
}

void browser_refresh(BrowserState* state) {
    BrowserTab* tab = browser_get_active_tab(state);
    if (tab && tab->url.length > 0) {
        browser_navigate(tab, tab->url.data);
    }
}

void browser_go_home(BrowserState* state) {
    BrowserTab* tab = browser_get_active_tab(state);
    if (tab) browser_navigate(tab, BROWSER_HOME_URL);
}

void browser_add_bookmark(BrowserState* state, const char* name, const char* url) {
    if (state->bookmark_count >= BROWSER_MAX_BOOKMARKS || !name || !url) return;
    state->bookmarks[state->bookmark_count].name = string_create(name);
    state->bookmarks[state->bookmark_count].url = string_create(url);
    state->bookmark_count++;
}

void browser_remove_bookmark(BrowserState* state, int index) {
    if (index < 0 || index >= state->bookmark_count) return;
    string_free(&state->bookmarks[index].name);
    string_free(&state->bookmarks[index].url);
    if (index < state->bookmark_count - 1) {
        memmove(&state->bookmarks[index], &state->bookmarks[index + 1],
                (state->bookmark_count - index - 1) * sizeof(Bookmark));
    }
    state->bookmark_count--;
}

void browser_add_history(BrowserState* state, const char* url, const char* title) {
    if (!url) return;
    if (state->history_count >= BROWSER_MAX_HISTORY) {
        string_free(&state->history[0].url);
        string_free(&state->history[0].title);
        memmove(&state->history[0], &state->history[1],
                (BROWSER_MAX_HISTORY - 1) * sizeof(HistoryEntry));
        state->history_count--;
    }
    state->history[state->history_count].url = string_create(url);
    state->history[state->history_count].title = string_create(title ? title : "");
    state->history[state->history_count].timestamp = time(NULL);
    state->history_count++;
    state->history_pos = state->history_count - 1;
}

void browser_update_layout(BrowserTab* tab, int viewport_width, int viewport_height) {
    if (!tab || !tab->document) return;
    render_layout_free(&tab->layout);
    tab->layout = render_document(tab->document, viewport_width, viewport_height);
}

static void browser_start_image_downloads(BrowserState* state, int tab_index) {
    if (tab_index < 0 || tab_index >= state->tab_count) return;
    BrowserTab* tab = &state->tabs[tab_index];
    if (!tab->document || tab->layout.image_count == 0) return;

    for (int i = 0; i < tab->layout.image_count && state->image_download_count < BROWSER_MAX_IMAGE_DOWNLOADS; i++) {
        RenderImage* img = &tab->layout.images[i];
        if (img->loaded || !img->href || !img->href[0]) continue;

        bool already_queued = false;
        for (int j = 0; j < state->image_download_count; j++) {
            if (state->image_downloads[j].tab_index == tab_index &&
                state->image_downloads[j].image_index == i) {
                already_queued = true;
                break;
            }
        }
        if (already_queued) continue;

        char full_url[2048] = {0};
        if (strncmp(img->href, "http://", 7) == 0 || strncmp(img->href, "https://", 8) == 0) {
            strncpy(full_url, img->href, sizeof(full_url) - 1);
        } else if (img->href[0] == '/') {
            const char* url = tab->url.data;
            const char* path_start = strstr(url, "://");
            if (path_start) {
                path_start += 3;
                const char* host_end = strchr(path_start, '/');
                if (host_end) {
                    int host_len = (int)(host_end - url);
                    strncpy(full_url, url, host_len);
                    full_url[host_len] = '\0';
                } else {
                    strncpy(full_url, url, sizeof(full_url) - 1);
                }
            }
            strncat(full_url, img->href, sizeof(full_url) - strlen(full_url) - 1);
        } else {
            const char* url = tab->url.data;
            const char* last_slash = strrchr(url, '/');
            if (last_slash) {
                int prefix_len = (int)(last_slash - url + 1);
                strncpy(full_url, url, prefix_len);
                full_url[prefix_len] = '\0';
            } else {
                strncpy(full_url, url, sizeof(full_url) - 1);
            }
            strncat(full_url, img->href, sizeof(full_url) - strlen(full_url) - 1);
        }
        if (full_url[0] == '\0') continue;

        ImageDownload* dl = &state->image_downloads[state->image_download_count];
        dl->tab_index = tab_index;
        dl->image_index = i;
        strncpy(dl->url, full_url, sizeof(dl->url) - 1);
        dl->client = http_client_create();
        if (dl->client && http_client_request(dl->client, full_url, HTTP_METHOD_GET)) {
            state->image_download_count++;
        } else {
            if (dl->client) http_client_destroy(dl->client);
            dl->client = NULL;
        }
    }
}

static bool is_image_content_type(const char* ct) {
    if (!ct) return false;
    return (strstr(ct, "image/png") || strstr(ct, "image/jpeg") ||
            strstr(ct, "image/gif") || strstr(ct, "image/webp") ||
            strstr(ct, "image/bmp") || strstr(ct, "image/x-icon") ||
            strstr(ct, "image/apng") || strstr(ct, "image/avif") ||
            strstr(ct, "image/svg"));
}

static bool sniff_image_from_bytes(const unsigned char* data, int len) {
    if (!data || len < 4) return false;
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return true;
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return true;
    if (data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8') return true;
    if (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') {
        if (len >= 12 && data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') return true;
    }
    if (data[0] == 'B' && data[1] == 'M') return true;
    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00) return true;
    return false;
}

static void browser_load_image(BrowserTab* tab, const unsigned char* data, int len, const char* content_type) {
    ImageData* idata = image_load_from_memory(data, len);
    if (idata && idata->pixels) {
        if (tab->image_pixels) free(tab->image_pixels);
        tab->image_pixels = idata->pixels;
        idata->pixels = NULL;
        tab->image_width = idata->width;
        tab->image_height = idata->height;
        tab->state = TAB_STATE_IMAGE;
        strncpy(tab->content_type, content_type ? content_type : "image/unknown", sizeof(tab->content_type) - 1);
        char title_buf[256];
        _snprintf(title_buf, sizeof(title_buf), "Image %dx%d", idata->width, idata->height);
        string_free(&tab->title);
        tab->title = string_create(title_buf);
        image_free(idata);
    } else {
        tab->state = TAB_STATE_ERROR;
        string_free(&tab->title);
        tab->title = string_create("Failed to decode image");
    }
}

/* --- Auto-redirect ("script") handling --------------------------------
 * Many sites (e.g. DuckDuckGo's interstitial) won't render for JS-less
 * browsers but emit a <meta http-equiv="refresh"> or a window.location /
 * location.href assignment to forward the client.  A full JavaScript
 * engine is not available on the Xbox 360, so instead we detect the most
 * common redirect patterns and follow them automatically.  This code is
 * shared by the Windows and Xbox 360 builds. */

static bool redirect_ci_equal(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static const char* redirect_strstr_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle || !needle[0]) return NULL;
    int nlen = (int)strlen(needle);
    while (*haystack) {
        if (redirect_ci_equal(haystack, needle)) return haystack;
        haystack++;
    }
    return NULL;
}

static bool redirect_copy_url(const char* p, char* out, int out_size) {
    if (!p || out_size <= 0) return false;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '\'' || *p == '"') {
        char q = *p;
        p++;
        const char* end = strchr(p, q);
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > (size_t)out_size - 1) len = out_size - 1;
        if (len == 0) return false;
        memcpy(out, p, len);
        out[len] = '\0';
        return true;
    }
    size_t i = 0;
    while (p[i] && p[i] != ';' && p[i] != ' ' && p[i] != '\t' &&
           p[i] != '"' && p[i] != '\'' && p[i] != '\n' && p[i] != '\r' && i < (size_t)out_size - 1) {
        out[i] = p[i];
        i++;
    }
    out[i] = '\0';
    return out[0] != '\0';
}

static bool redirect_from_meta_content(const char* content, char* out, int out_size) {
    if (!content || !content[0]) return false;
    const char* url_eq = redirect_strstr_ci(content, "url=");
    if (url_eq) return redirect_copy_url(url_eq + 4, out, out_size);
    const char* semi = strrchr(content, ';');
    const char* start = semi ? semi + 1 : content;
    if (!start || !start[0] || start[0] == '\'') return false;
    return redirect_copy_url(start, out, out_size);
}

static bool redirect_walk(HtmlNode* node, char* out, int out_size) {
    if (!node) return false;
    if (node->type == HTML_NODE_ELEMENT) {
        if (node->tag == HTML_TAG_META) {
            const char* he = html_node_get_attribute(node, "http-equiv");
            if (he && redirect_ci_equal(he, "refresh")) {
                const char* content = html_node_get_attribute(node, "content");
                if (content && redirect_from_meta_content(content, out, out_size)) return true;
            }
        }
    }
    for (int i = 0; i < node->child_count; i++) {
        if (redirect_walk(node->children[i], out, out_size)) return true;
    }
    return false;
}

static bool redirect_resolve(const char* current, const char* target, char* out, int out_size) {
    if (!target || !target[0] || target[0] == '#') return false;
    if (strncmp(target, "http://", 7) == 0 || strncmp(target, "https://", 8) == 0) {
        strncpy(out, target, out_size - 1);
        out[out_size - 1] = '\0';
        return true;
    }
    if (target[0] == '/' && current) {
        const char* scheme_end = strstr(current, "://");
        if (scheme_end) {
            int prefix = (int)(scheme_end + 3 - current);
            const char* host_end = strchr(scheme_end + 3, '/');
            if (host_end) prefix = (int)(host_end - current);
            if (prefix + (int)strlen(target) < out_size) {
                memcpy(out, current, prefix);
                strcpy(out + prefix, target);
                return true;
            }
        }
    }
    return false;
}

void browser_update(BrowserState* state) {
    if (!state) return;
    const PlatformAPI* api = platform_get_api();

    for (int i = 0; i < state->tab_count; i++) {
        BrowserTab* tab = &state->tabs[i];

        if (tab->state == TAB_STATE_LOADING && tab->http_client) {
            http_client_poll(tab->http_client);

            if (http_client_is_done(tab->http_client)) {
                HttpResponse* resp = http_client_get_response(tab->http_client);

                if (resp && resp->status_code >= 300 && resp->status_code < 400) {
                    const char* location = hashmap_get(&resp->headers, "Location");
                    if (!location) location = hashmap_get(&resp->headers, "location");
                    if (location && location[0]) {
                        char resolved[2048] = {0};
                        if (strncmp(location, "http://", 7) == 0 || strncmp(location, "https://", 8) == 0) {
                            strncpy(resolved, location, sizeof(resolved) - 1);
                        } else {
                            const char* url = tab->url.data;
                            const char* scheme_end = strstr(url, "://");
                            if (scheme_end) {
                                int prefix = (int)(scheme_end + 3 - url);
                                const char* host_end = strchr(scheme_end + 3, '/');
                                if (host_end) prefix = (int)(host_end - url);
                                if (prefix < (int)sizeof(resolved) - 1) {
                                    strncpy(resolved, url, prefix);
                                    resolved[prefix] = '\0';
                                }
                            }
                            strncat(resolved, location, sizeof(resolved) - strlen(resolved) - 1);
                        }
                        http_client_destroy(tab->http_client);
                        tab->http_client = NULL;
                        browser_navigate(tab, resolved);
                        continue;
                    }
                }

                if (resp && resp->status_code == 200) {
                    const char* ct = resp->content_type.data;
                    bool is_image = false;
                    if (ct && is_image_content_type(ct)) {
                        is_image = true;
                    } else if (resp->body.length >= 4) {
                        is_image = sniff_image_from_bytes((const unsigned char*)resp->body.data, resp->body.length);
                    }

                    if (is_image) {
                        browser_load_image(tab, (const unsigned char*)resp->body.data,
                                          resp->body.length, ct);
                    } else {
                        tab->http_status = resp->status_code;
                        tab->body_len = resp->body.length;
                        tab->body_gzip = (resp->body.length >= 2 &&
                            (unsigned char)resp->body.data[0] == 0x1F &&
                            (unsigned char)resp->body.data[1] == 0x8B) ? 1 : 0;
                        tab->body_chunked = resp->chunked ? 1 : 0;
                        {
                            int pi = 0;
                            for (int k = 0; k < 70 && k < resp->body.length; k++) {
                                unsigned char c = (unsigned char)resp->body.data[k];
                                tab->body_prefix[pi++] = (c >= 32 && c != 127) ? (char)c : '.';
                            }
                            tab->body_prefix[pi] = '\0';
                        }
                        if (tab->document) html_document_free(tab->document);
                        tab->document = html_parse(resp->body.data, resp->body.length);
                        if (tab->document) {
                            /* Auto-follow <meta http-equiv="refresh"> and
                             * window.location / location.href redirects so
                             * JS-less (interstitial) pages forward correctly. */
                            char redir[2048] = {0};
                            if (tab->document->root && redirect_walk(tab->document->root, redir, sizeof(redir))) {
                                char resolved[2048] = {0};
                                if (redirect_resolve(tab->url.data, redir, resolved, sizeof(resolved))) {
                                    html_document_free(tab->document);
                                    tab->document = NULL;
                                    http_client_destroy(tab->http_client);
                                    tab->http_client = NULL;
                                    browser_navigate(tab, resolved);
                                    continue;
                                }
                            }
                            /* Native translation: rewrite foreign-language text
                             * nodes to English using the offline dictionary
                             * before the layout is built. */
                            translate_document(tab->document);
                            if (tab->document->title.length > 0) {
                                string_free(&tab->title);
                                tab->title = string_clone(tab->document->title);
                            }
                            browser_update_layout(tab, state->viewport_width, state->viewport_height - state->ui_bar_height);
                        }
                        tab->state = TAB_STATE_LOADED;
                    }
                    browser_add_history(state, tab->url.data, tab->title.data);
                } else if (resp) {
                    tab->state = TAB_STATE_ERROR;
                    string_free(&tab->title);
                    char buf[128];
                    _snprintf(buf, sizeof(buf), "Error %d", resp->status_code);
                    tab->title = string_create(buf);
                } else {
                    tab->state = TAB_STATE_ERROR;
                    string_free(&tab->title);
                    tab->title = string_create("Connection failed");
                }

                http_client_destroy(tab->http_client);
                tab->http_client = NULL;
                if (tab->state == TAB_STATE_LOADED && tab->document)
                    browser_start_image_downloads(state, i);
            }
        }
    }

    for (int i = state->image_download_count - 1; i >= 0; i--) {
        ImageDownload* dl = &state->image_downloads[i];
        if (!dl->client) { state->image_download_count--; continue; }

        http_client_poll(dl->client);

        if (http_client_is_done(dl->client)) {
            HttpResponse* resp = http_client_get_response(dl->client);
            if (dl->tab_index < state->tab_count) {
                BrowserTab* tab = &state->tabs[dl->tab_index];
                if (tab->state == TAB_STATE_LOADED && dl->image_index < tab->layout.image_count) {
                    RenderImage* img = &tab->layout.images[dl->image_index];
                    if (resp && resp->status_code == 200 && resp->body.length > 0) {
                        ImageData* idata = image_load_from_memory((const uint8_t*)resp->body.data, resp->body.length);
                        if (idata) {
                            img->pixels = idata->pixels;
                            idata->pixels = NULL;
                            img->width = idata->width;
                            img->height = idata->height;
                            img->loaded = true;
                            image_free(idata);
                        }
                    }
                }
            }
            http_client_destroy(dl->client);
            dl->client = NULL;
            if (i < state->image_download_count - 1) {
                state->image_downloads[i] = state->image_downloads[state->image_download_count - 1];
            }
            state->image_download_count--;
        }
    }
}
