import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from dreamtool import build
from hudpainter_export import export_preset

class CompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.obj=tempfile.TemporaryDirectory()
        self.addCleanup(self.obj.cleanup)
        self.tmp=Path(self.obj.name)
        self.preset={
            "propertiesDefault":[
                {"name":"MainColors.Red","red":1.1761,"green":.4,"blue":.35,"alpha":.6},
                {"name":"MainColors.PanelBlue","red":.4,"green":.95,"blue":1.0,"alpha":.8},
                {"name":"MainColors.ActiveYellow","red":1.331,"green":1.01,"blue":.3,"alpha":1.0},
                {"name":"MainColors.White","red":1.0,"green":1.0,"blue":1.0,"alpha":1.0}],
            "propertiesJohnny":[{"name":"MainColors.Red","red":2.4,"green":.2,"blue":.3,"alpha":.55}]}
        self.baseline=self.tmp/"DEFAULT.json"
        self.save()
    def save(self):
        self.baseline.write_text(json.dumps(self.preset),encoding="utf-8")
    def test_complete_preset_and_rollback_by_default(self):
        dest=self.tmp/"DreamUI.json"
        changes=export_preset(ROOT/"themes/Default",self.baseline,dest)
        output=json.loads(dest.read_text(encoding="utf-8"))
        self.assertEqual(len(changes),3)
        self.assertEqual(output["propertiesJohnny"],self.preset["propertiesJohnny"])
        self.assertEqual(output["propertiesDefault"][3],self.preset["propertiesDefault"][3])
        self.assertAlmostEqual(output["propertiesDefault"][0]["red"],234/255*1.1761,places=7)
        self.assertAlmostEqual(output["propertiesDefault"][1]["blue"],179/255,places=7)
        self.assertAlmostEqual(output["propertiesDefault"][2]["red"],246/255*1.331,places=7)
        for n in range(3):
            self.assertEqual(output["propertiesDefault"][n]["alpha"],self.preset["propertiesDefault"][n]["alpha"])
        self.assertEqual(json.loads(self.baseline.read_text()),self.preset)
        with self.assertRaisesRegex(ValueError,"destination already exists"):
            export_preset(ROOT/"themes/Default",self.baseline,dest)
    def test_packed_and_folder_match(self):
        package=self.tmp/"Default.dream"
        build(ROOT/"themes/Default",package)
        a,b=self.tmp/"folder.json",self.tmp/"packed.json"
        export_preset(ROOT/"themes/Default",self.baseline,a)
        export_preset(package,self.baseline,b)
        self.assertEqual(a.read_bytes(),b.read_bytes())
    def test_invalid_baseline_rejected(self):
        self.preset["propertiesDefault"].pop(1)
        self.save()
        with self.assertRaisesRegex(ValueError,"missing required native"):
            export_preset(ROOT/"themes/Default",self.baseline,self.tmp/"out.json")
        self.preset["propertiesDefault"].insert(1,dict(self.preset["propertiesDefault"][0]))
        self.save()
        with self.assertRaisesRegex(ValueError,"duplicated"):
            export_preset(ROOT/"themes/Default",self.baseline,self.tmp/"out.json")
        self.preset["propertiesDefault"].pop(1)
        self.preset["propertiesDefault"][0]["red"]=float("nan")
        self.save()
        with self.assertRaisesRegex(ValueError,"invalid HDRColor"):
            export_preset(ROOT/"themes/Default",self.baseline,self.tmp/"out.json")
    def test_missing_color_rejected(self):
        source=self.tmp/"theme"
        shutil.copytree(ROOT/"themes/Default",source)
        color=source/"theme/colors.json"
        data=json.loads(color.read_text())
        del data["text"]["secondary"]
        color.write_text(json.dumps(data),encoding="utf-8")
        with self.assertRaisesRegex(ValueError,"text.secondary"):
            export_preset(source,self.baseline,self.tmp/"out.json")
    def test_no_overwrite(self):
        with self.assertRaisesRegex(ValueError,"refusing to overwrite"):
            export_preset(ROOT/"themes/Default",self.baseline,self.baseline)
    def test_bad_zip_never_written(self):
        file=self.tmp/"bad.dream"
        file.write_bytes(b"garbage")
        output=self.tmp/"out.json"
        with self.assertRaises(Exception):
            export_preset(file,self.baseline,output)
        self.assertFalse(output.exists())

if __name__=="__main__":
    unittest.main()
