-- Finite connection/relation input for tools/create_workbook.py.
-- Consumer: tests/support/table_replacement_app.inc; @SCALE@ is 1 or 10.
CREATE TABLE alpha(City VARCHAR, Amount DECIMAL(12,2));
INSERT INTO alpha VALUES ('Ankara',10*@SCALE@),('Ankara',20*@SCALE@),('Bursa',30*@SCALE@);
CREATE TABLE beta(City VARCHAR, Amount DECIMAL(12,2));
INSERT INTO beta VALUES ('Ankara',100*@SCALE@),('Bursa',200*@SCALE@);
CREATE VIEW beta_view AS SELECT * FROM beta;
