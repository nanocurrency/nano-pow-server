/** Renders work queue information */
export default class Queue {
    constructor(app) {
        this.app = app;
        this.queue_update_interval = 3000;
        this.avg_work_time = 0;
    }

    init() {
        this.app.router.on('/queue', (params) => {
            this.queue_show ();
            this.app.show_page('page_queue');

            $("#table_active_empty").hide();
            $("#table_queued_empty").hide();
            $("#table_completed_empty").hide();
            $("#status_avg_work").hide();
        });
    }

    /** Show the queue */
    queue_show() {
        this.app.show_spinner();
        this.app.hide_alert();

        $.getJSON("/api/v1/work/queue", (data) => {
            // console.log(data);
            this.app.hide_spinner();

            if (Array.isArray(data.active) && data.active.length > 0) {
                let tbl = this.queue_to_table (data.active, "active");
                $("#table_active").empty();
                $("#table_active").append(this.table(tbl.header, tbl.rows));

                $("#table_active_empty").hide();
                $("#table_active").show();
            }
            else {
                $("#table_active_empty").show();
                $("#table_active").hide();
            }

            if (Array.isArray(data.queued) && data.queued.length > 0) {
                let tbl = this.queue_to_table (data.queued, "pending");
                $("#table_queued").empty();
                $("#table_queued").append(this.table(tbl.header, tbl.rows));

                $("#table_queued_empty").hide();
                $("#table_queued").show();
            }
            else {
                $("#table_queued_empty").show();
                $("#table_queued").hide();
            }
            if (Array.isArray(data.completed) && data.completed.length > 0) {
                let tbl = this.queue_to_table (data.completed.reverse(), "completed");
                $("#table_completed").empty();
                $("#table_completed").append(this.table(tbl.header, tbl.rows));

                $("#table_completed_empty").hide();
                $("#table_completed").show();

                this.avg_work_time = data.completed.reduce((a,b) =>
                    ( {duration: a.duration + b.duration })).duration / data.completed.length;
                this.avg_work_time = Math.round(this.avg_work_time);
                if (this.avg_work_time > 0){
                    $("#avg_work_value").html(''+this.avg_work_time);
                    $("#status_avg_work").show();
                }
            }
            else {
                $("#table_completed_empty").show();
                $("#table_completed").hide();
            }
        });

        if (this.queue_update_interval) {
            setTimeout(() => this.queue_show(), this.queue_update_interval);
        }
    }

    queue_clear() {
        $.ajax({
            type: "DELETE",
            url: "/api/v1/work/queue",
            success: (data) => {
                    console.log(data);
                    if (data.success)
                    {
                        this.app.show_message('The queue was successfully cleared');
                    }
                    else if (data.error)
                    {
                        this.app.show_alert(data.error);
                    }
            }
        });
    }

    queue_to_table (queue, queue_type) {
        let header = ""
        if (queue_type == "pending") {
            header = this.header_col("priority");
        }
        else if (queue_type == "active") {
            header = this.header_col("start");
        }
        else {
            header = this.header_col("start") + this.header_col("end") + this.header_col("duration");
        }
        header += this.header_col("difficulty") + this.header_col("multiplier") + this.header_col("hash");

        let rows = "";
        for (const job of queue) {
            console.log(job)
            let cols = "";
            if (queue_type == "pending") {
                if (!job.priority || parseInt(job.priority) == 0) {
                    cols += this.col("Normal");
                }
                else {
                    cols += this.col("+"+job.priority);
                }
            }
            else if (queue_type == "active") {
                cols += this.col(this.date_fmt(job.start));
            }
            else {
                cols += this.col(this.date_fmt(job.start));
                cols += this.col(this.date_fmt(job.end, true));
                const duration = job.end - job.start;
                job.duration = duration;
                cols += this.col(duration + " ms");
            }
            cols += this.col(job.request.difficulty);
            cols += this.col(job.request.multiplier);
            cols += this.col(job.request.hash, true);

            let row = this.row(cols);
            rows += row;
        }
        return {rows: rows, header: header}
    }

    on_query_content_table (self, event, content_table, content_id) {
        event.preventDefault();
        this.app.router.navigate(`/${content_table}/${content_id}`);
    }

    table (header_cols, rows) {
        return `
            <table class="table table-sm">
              <thead>
                <tr>
                  ${header_cols}
                </tr>
              </thead>
              <tbody>
                ${rows}
              </tbody>
             </table>
        `;
    }

    header_col (name) {
        return `<th scope="col">${name}</td>`;
    }

    row (cols) {
        return `<tr>${cols}</tr>`;
    }

    col (val, is_hash, link, hover) {
        let col = `<td nowrap`;
        if (hover) {
            col += ` title="${hover}"`;
        }

        if (is_hash) {
            col += `><a href="https://www.nanode.co/block/${val}" target="_blank">${val}</a>`;
        }
        else if (link) {
            col += `><a href="#" onclick="${link}">${val}</a>`;
        }
        else {
            col += `>${val}`;
        }
        col += `</td>`
        return col;
    }

    /** Time if today, otherwise date and time */
    date_fmt (date, force_time_only) {
        const dt = moment(new Date(parseInt(date)));
        if (force_time_only || dt.isSame(new Date(), "day")) {
            console.log(dt)
            return dt.format("HH:mm:ss.SSS");
        }
        else {
            return dt.format("L HH:mm:ss.SSS");
        }
    }
}
