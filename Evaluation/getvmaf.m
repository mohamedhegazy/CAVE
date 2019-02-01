function [val]=getvmaf(file,frames)
import javax.xml.xpath.*
factory = XPathFactory.newInstance;
xpath = factory.newXPath;
DOMnode = xmlread(file);
dictExpr = xpath.compile('//fyi/@aggregateVMAF');
allDict = dictExpr.evaluate(DOMnode, XPathConstants.NODESET);
val=str2double(allDict.item(0).getFirstChild.getNodeValue);
