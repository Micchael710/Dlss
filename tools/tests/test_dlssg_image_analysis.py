"""Synthetic CPU motion fixtures; no claim of GPU generation."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from analyze_dlssg_image_capture import temporal,cv2,np
class MotionTest(unittest.TestCase):
    def test_order_and_duplicates(self):
        rng=np.random.default_rng(103)
        rgb=cv2.GaussianBlur(rng.integers(0,256,(360,640,3),np.uint8),(5,5),0)
        a=np.concatenate([rgb,np.full((360,640,1),255,np.uint8)],axis=2)
        shift=lambda n:cv2.warpAffine(a,np.float32([[1,0,n],[0,1,0]]),(640,360),borderMode=cv2.BORDER_REFLECT)
        result=temporal(a,shift(12),{'G1':shift(4),'G2':shift(8)})
        self.assertTrue(result['ordered_motion_estimate'])
        self.assertAlmostEqual(result['G1']['tau_median'],1/3,delta=.1)
        self.assertAlmostEqual(result['G2']['tau_median'],2/3,delta=.1)
        self.assertFalse(temporal(a,shift(12),{'G1':shift(4),'G2':shift(4)})['ordered_motion_estimate'])
        self.assertFalse(temporal(a,shift(12),{'G1':shift(8),'G2':shift(4)})['ordered_motion_estimate'])
if __name__=='__main__':unittest.main()
