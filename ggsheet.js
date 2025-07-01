const SHEET_NAME = "TestLoadCell1KG";


function doGet(e) {
  try {
    const spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = spreadsheet.getSheetByName(SHEET_NAME);

    if (!sheet) {
      sheet = spreadsheet.insertSheet(SHEET_NAME);
      sheet.appendRow(["Ngày", "Giờ", "Số thứ tự", "Khối lượng"]);
    }

    const stt = e.parameter.stt;
    const khoiluong = e.parameter.khoiluong;

    if (!stt || !khoiluong) {
      return ContentService
        .createTextOutput("Lỗi: Thiếu tham số 'stt' hoặc 'khoiluong'.")
        .setMimeType(ContentService.MimeType.TEXT);
    }
    
    const now = new Date();
    const date = Utilities.formatDate(now, "GMT+7", "dd/MM/yyyy");
    const time = Utilities.formatDate(now, "GMT+7", "HH:mm:ss");

    sheet.appendRow([date, time, stt, khoiluong]);

    return ContentService
      .createTextOutput("Dữ liệu đã được ghi thành công!")
      .setMimeType(ContentService.MimeType.TEXT);

  } catch (error) {
    Logger.log(error.toString());
    return ContentService
      .createTextOutput("Đã xảy ra lỗi: " + error.toString())
      .setMimeType(ContentService.MimeType.TEXT);
  }
}
