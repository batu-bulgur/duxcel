#ifndef DUXCEL_TEST_CURRENT_ARCHIVE_H
#define DUXCEL_TEST_CURRENT_ARCHIVE_H
#include "workbook/version_save.h"
#include "platform/directory.h"
#include "platform/platform.h"
#include <stdlib.h>
/* In-test authored models use the real current Save path. This is not a
   standalone fixture generator or an alternate implementation of the format. */
static bool test_current_archive_save(const Workbook *book,const char *path,char **error) {
    char *parent=dx_temp_directory();
    if (!parent) return false;
    WorkbookVersionReceipt *receipt=NULL;
    WorkbookVersionSaveRequest request={.head=book,.owner={.document_high=book->id.high,.document_low=book->id.low,.session=1,.mode=VERSION_WORKING_HEAD},
        .purpose=VERSION_USER_SAVE,.path=path,.parent=parent,.reserved_bytes=512u*1024u*1024u};
    bool ok=workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,error)==WORKBOOK_SAVE_COMPLETE;
    workbook_version_receipt_free(receipt); dx_remove_temp_directory(parent); free(parent); return ok;
}
#endif
