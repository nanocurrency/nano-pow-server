import Queue from '/app_queue.js';
import Router from '/app_router.js';

/** App UI */
export default class App {
    constructor () {
        this.active_page = 'queue';
        this.spinner_started = false;
        this.status_interval = 1200;
        this.router = new Router();
        this.common = new Common(this);
        this.queue = new Queue(this);
    }

    /** Set up click handlers and initialize components. This is called after
    the page is fully loaded. */
    init() {
        console.log('Client version', this.common.version);

        $('#action_queue_clear').click(() => {
            this.queue.queue_clear();
        });

        // Show server version
        $.getJSON("/api/v1/version", (data) => {
            if (data.version) {
                console.log('Server version', data.version);
                $('#header_title').prop ("title", "Version " + data.version);
            }
        });

        $('#action_server_stop').click(() => {
            $.getJSON("/api/v1/stop", (data) => {
                if (data.error) {
                    this.show_alert(data.error)
                }
                else if (data.success) {
                    this.show_message("Server was successfully stopped");
                }
            });
        });

        this.queue.init();
        this.update_status_loop();

        if (window.location.pathname == '/') {
            this.router.navigate('/queue');
        }
        else {
            this.router.navigate('/'+window.location.pathname);
        }
    }

    show_spinner () {
        // This may be set to false before the timer starts
        this.spinner_started = true;
        setTimeout(() => {
            if (this.spinner_started) {
                $('#app_spinner').removeClass('d-none');
            }
        }, 500);
    }

    hide_spinner () {
        this.spinner_started = false;
        $('#app_spinner').addClass('d-none');
    }

    /** Show \error_message for the duration of \visible_time_ms */
    show_alert (error_message, visible_time_ms) {
        $('#app_alert_text').empty();
        $('#app_alert_text').append(error_message);
        $('#app_alert_area').removeClass('d-none');

        setTimeout(() => {
            this.hide_alert();
        }, visible_time_ms ? visible_time_ms : 3000);
    }

    hide_alert () {
        $('#app_alert_area').addClass('d-none');
    }

    /** Show \success_message for the duration of \visible_time_ms */
    show_message (success_message, visible_time_ms) {
        $('#app_success_text').empty();
        $('#app_success_text').append(success_message);
        $('#app_success_area').removeClass('d-none');

        setTimeout(() => {
            this.hide_message();
        }, visible_time_ms ? visible_time_ms : 3000);
    }

    hide_message () {
        $('#app_success_area').addClass('d-none');
    }

    /** Show the "page_container" child div with the given id */
    show_page (page_id) {
        if (this.active_page == page_id)
            return;
        $('#page_container').children('div').each((index) => {
            const div = $('#page_container').children().eq(index);
            const div_id = div.attr("id");

            if (div_id === page_id) {
                $('#'+div_id).removeClass('d-none');
                $('#nav_'+div_id).addClass('active');
                this.active_page = page_id;
                eval('this.on_' + page_id + '();');
            }
            else {
                $('#'+div_id).addClass('d-none');
                $('#nav_'+div_id).removeClass('active');
            }
        });
    }

    /** Status loop with interval back-off if the server is down */
    update_status_loop () {
        $.getJSON("/api/v1/ping")
        .done((data) => {
            if (data) {
                for (const name in data) {
                    if (data[name]) {
                        $('#status_'+name).removeClass('d-none');
                        $('#status_'+name+"_inverted").addClass('d-none');
                    }
                    else {
                        $('#status_'+name).addClass('d-none');
                        $('#status_'+name+"_inverted").removeClass('d-none');
                    }
                }
            }
            this.status_interval = 1200;
            $('#status_network').addClass('d-none');
        })
        .fail((jqXHR, textStatus, errorThrown) => {
                $('#status_network').removeClass('d-none');
                // The status call is cheap so we don't back off more than to 3 secs to give quick
                // feedback when the process is available again.
                if (this.status_interval < 3000) {
                    this.status_interval *= 2;
                }
            })
        .always(() => {});

        if (this.status_interval) {
            setTimeout(() => this.update_status_loop(), this.status_interval);
        }
    }

    on_page_home () {
    }

    on_page_queue () {
    }

    on_page_config () {
    }
}

export class Common {
    constructor(app) {
        this.app = app;
        this.version = "1.0.0";
    }
}
