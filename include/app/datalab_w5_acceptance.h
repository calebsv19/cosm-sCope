#ifndef DATALAB_W5_ACCEPTANCE_H
#define DATALAB_W5_ACCEPTANCE_H

/* Explicit, non-interactive acceptance entry point.  It is never reached by
 * the normal launcher and writes fixtures/metrics only below output_root. */
int datalab_w5_acceptance_run(const char *output_root);

#endif
